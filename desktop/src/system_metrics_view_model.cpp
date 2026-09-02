#include <guardian/desktop/system_metrics_view_model.hpp>

#include <QDateTime>
#include <QStringList>
#include <QTimer>

#include <utility>

namespace guardian::desktop {

namespace {

constexpr double bytes_per_gib = 1024.0 * 1024.0 * 1024.0;
constexpr qint64 stale_after_ms = 15000;

}  // namespace

SystemMetricsViewModel::SystemMetricsViewModel(QObject* parent)
    : SystemMetricsViewModel(
          QStringLiteral("ai_pc_guardian_v1"),
          parent) {
}

SystemMetricsViewModel::SystemMetricsViewModel(
    QString agent_server_name,
    QObject* parent)
    : QObject(parent),
      client_(std::move(agent_server_name)) {
    connect(
        &client_,
        &LocalAgentClient::snapshot_received,
        this,
        &SystemMetricsViewModel::apply_snapshot);
    connect(
        &client_,
        &LocalAgentClient::connection_state_changed,
        this,
        &SystemMetricsViewModel::apply_connection_state);
    connect(
        &client_,
        &LocalAgentClient::protocol_error,
        this,
        [this](const QString& message) {
            status_text_ = message;
            refreshing_ = false;
            emit state_changed();
        });

    QTimer::singleShot(0, &client_, &LocalAgentClient::start);
}

SystemMetricsViewModel::~SystemMetricsViewModel() {
    client_.stop();
}

double SystemMetricsViewModel::cpu_usage() const noexcept {
    return cpu_usage_;
}

double SystemMetricsViewModel::memory_usage() const noexcept {
    return memory_usage_;
}

double SystemMetricsViewModel::memory_used_gib() const noexcept {
    return memory_used_gib_;
}

double SystemMetricsViewModel::memory_total_gib() const noexcept {
    return memory_total_gib_;
}

QString SystemMetricsViewModel::status_text() const {
    return status_text_;
}

QString SystemMetricsViewModel::last_updated() const {
    return last_updated_;
}

bool SystemMetricsViewModel::refreshing() const noexcept {
    return refreshing_;
}

bool SystemMetricsViewModel::agent_available() const noexcept {
    return agent_available_;
}

bool SystemMetricsViewModel::data_stale() const noexcept {
    return data_stale_;
}

void SystemMetricsViewModel::refresh() {
    if (!agent_available_ || refreshing_) {
        return;
    }

    refreshing_ = true;
    status_text_ = QStringLiteral("Запрашиваем данные Agent...");
    emit state_changed();
    client_.request_snapshot();
}

void SystemMetricsViewModel::apply_snapshot(LocalAgentSnapshot snapshot) {
    refreshing_ = false;

    if (!snapshot.has_snapshot) {
        data_stale_ = has_snapshot_;
        status_text_ = has_snapshot_
            ? QStringLiteral(
                  "Ожидание первого измерения · данные устарели")
            : QStringLiteral("Ожидание первого измерения");
        emit state_changed();
        return;
    }

    has_snapshot_ = true;
    const qint64 snapshot_age_ms =
        QDateTime::currentMSecsSinceEpoch() -
        snapshot.collected_at_unix_ms;
    data_stale_ = snapshot.collected_at_unix_ms <= 0 ||
        snapshot_age_ms > stale_after_ms;
    QStringList partial_errors;

    if (snapshot.cpu) {
        cpu_usage_ = snapshot.cpu->usage_percent;
    } else {
        partial_errors.push_back(QStringLiteral("CPU недоступен"));
    }

    if (snapshot.memory) {
        const auto used_bytes = snapshot.memory->used_bytes();
        memory_used_gib_ = static_cast<double>(used_bytes) / bytes_per_gib;
        memory_total_gib_ =
            static_cast<double>(snapshot.memory->total_bytes) / bytes_per_gib;
        memory_usage_ = memory_total_gib_ > 0.0
            ? memory_used_gib_ / memory_total_gib_ * 100.0
            : 0.0;
    } else {
        partial_errors.push_back(QStringLiteral("RAM недоступна"));
    }

    if (snapshot.state == LocalAgentState::Running &&
        partial_errors.empty()) {
        status_text_ = QStringLiteral("Система под наблюдением");
    } else if (!partial_errors.empty()) {
        status_text_ = partial_errors.join(QStringLiteral(" · "));
    } else {
        status_text_ = snapshot.status_message.isEmpty()
            ? QStringLiteral("Agent работает с ограничениями")
            : QStringLiteral("Agent работает с ограничениями · %1")
                  .arg(snapshot.status_message);
    }

    if (data_stale_) {
        status_text_ += QStringLiteral(" · данные устарели");
    }

    last_updated_ = QDateTime::fromMSecsSinceEpoch(
        snapshot.collected_at_unix_ms)
        .toLocalTime()
        .toString(QStringLiteral("HH:mm:ss"));

    emit metrics_changed();
    emit state_changed();
}

void SystemMetricsViewModel::apply_connection_state(
    LocalAgentClient::ConnectionState state) {
    switch (state) {
    case LocalAgentClient::ConnectionState::Connecting:
        agent_available_ = false;
        refreshing_ = true;
        status_text_ = QStringLiteral("Подключение к Agent...");
        break;
    case LocalAgentClient::ConnectionState::Connected:
        agent_available_ = true;
        refreshing_ = true;
        status_text_ = QStringLiteral("Ожидание ответа Agent...");
        break;
    case LocalAgentClient::ConnectionState::Disconnected:
        agent_available_ = false;
        refreshing_ = false;
        data_stale_ = has_snapshot_;
        status_text_ = has_snapshot_
            ? QStringLiteral("Agent недоступен · данные устарели")
            : QStringLiteral("Agent недоступен");
        break;
    }

    emit state_changed();
}

}  // namespace guardian::desktop
