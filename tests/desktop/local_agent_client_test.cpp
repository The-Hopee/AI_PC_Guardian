#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>

#include <guardian/agent/local_ipc_server.hpp>
#include <guardian/desktop/local_agent_client.hpp>
#include <guardian/desktop/system_metrics_view_model.hpp>

#include <QCoreApplication>
#include <QDateTime>
#include <QEventLoop>
#include <QString>
#include <QTimer>

#include <chrono>
#include <functional>
#include <iostream>
#include <string_view>
#include <utility>

namespace {

int failure_count = 0;

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        ++failure_count;
    }
}

bool wait_until(
    const std::function<bool()>& condition,
    int timeout_ms = 5000) {
    if (condition()) {
        return true;
    }

    QEventLoop loop;
    QTimer poll;
    poll.setInterval(10);
    QObject::connect(&poll, &QTimer::timeout, &loop, [&] {
        if (condition()) {
            loop.quit();
        }
    });
    QTimer::singleShot(timeout_ms, &loop, &QEventLoop::quit);
    poll.start();
    loop.exec();
    return condition();
}

}  // namespace

int main(int argc, char* argv[]) {
    QCoreApplication application{argc, argv};

    const QString server_name =
        QStringLiteral("ai_pc_guardian_qt_test_%1")
            .arg(GetCurrentProcessId());
    const std::wstring pipe_name =
        (QStringLiteral(R"(\\.\pipe\)") + server_name).toStdWString();

    guardian::agent::LatestSnapshotStore store;
    store.update(guardian::agent::LatestSnapshot{
        guardian::model::Timestamp{std::chrono::milliseconds{987654}},
        guardian::model::CpuMetric{31.25},
        guardian::model::MemoryMetric{32000, 12000},
        {},
        {}});

    guardian::agent::IpcServer server{store, pipe_name};
    guardian::desktop::LocalAgentClient client{server_name};

    int snapshot_count = 0;
    int connected_count = 0;
    int disconnected_count = 0;
    guardian::desktop::LocalAgentSnapshot latest;

    QObject::connect(
        &client,
        &guardian::desktop::LocalAgentClient::connection_state_changed,
        [&](guardian::desktop::LocalAgentClient::ConnectionState state) {
            if (state == guardian::desktop::LocalAgentClient::ConnectionState::Connected) {
                ++connected_count;
            }
            if (state == guardian::desktop::LocalAgentClient::ConnectionState::Disconnected) {
                ++disconnected_count;
            }
        });
    QObject::connect(
        &client,
        &guardian::desktop::LocalAgentClient::snapshot_received,
        [&](guardian::desktop::LocalAgentSnapshot snapshot) {
            latest = std::move(snapshot);
            ++snapshot_count;
        });

    server.start();
    client.start();

    expect(wait_until([&] { return snapshot_count >= 1; }),
           "Qt client receives the first snapshot asynchronously");
    expect(connected_count == 1,
           "Qt client establishes one connection");
    expect(latest.state == guardian::desktop::LocalAgentState::Running,
           "Qt client maps RUNNING state");
    expect(latest.has_snapshot && latest.cpu && latest.memory,
           "Qt client maps complete metrics");
    if (latest.cpu && latest.memory) {
        expect(latest.cpu->usage_percent == 31.25,
               "Qt client preserves CPU usage");
        expect(latest.memory->total_bytes == 32000 &&
                   latest.memory->available_bytes == 12000,
               "Qt client preserves memory values");
    }

    client.request_snapshot();
    expect(wait_until([&] { return snapshot_count >= 2; }),
           "Qt client reuses the connection for a second request");
    expect(connected_count == 1,
           "second request does not reconnect");

    server.stop();
    expect(wait_until([&] { return disconnected_count >= 1; }),
           "Qt client detects Agent shutdown");

    server.start();
    expect(wait_until([&] { return snapshot_count >= 3; }, 7000),
           "Qt client reconnects after Agent restart");
    expect(connected_count >= 2,
           "reconnect establishes a new connection");

    client.stop();

    store.update(guardian::agent::LatestSnapshot{
        guardian::model::Timestamp{
            std::chrono::milliseconds{
                QDateTime::currentMSecsSinceEpoch()}},
        guardian::model::CpuMetric{55.5},
        guardian::model::MemoryMetric{64000, 16000},
        {},
        {}});

    {
        guardian::desktop::SystemMetricsViewModel view_model{server_name};
        expect(wait_until([&] {
                   return view_model.agent_available() &&
                       view_model.cpu_usage() == 55.5;
               }),
               "ViewModel receives metrics only through the IPC client");

        const double last_cpu = view_model.cpu_usage();
        server.stop();
        expect(wait_until([&] {
                   return !view_model.agent_available() &&
                       view_model.data_stale();
               }),
               "ViewModel marks preserved data as stale after disconnect");
        expect(view_model.cpu_usage() == last_cpu,
               "ViewModel preserves the last metric after disconnect");
    }

    server.stop();

    if (failure_count != 0) {
        std::cerr << failure_count << " test(s) failed\n";
        return 1;
    }
    return 0;
}
