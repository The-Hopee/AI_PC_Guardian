#include <guardian/desktop/local_agent_client.hpp>

#include <guardian/ipc/framing.hpp>

#include <guardian/v1/local_agent.pb.h>

#include <QIODevice>

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <utility>

namespace guardian::desktop {

namespace {

constexpr std::uint32_t protocol_version = 1;
constexpr int poll_interval_ms = 3000;
constexpr std::array<int, 4> reconnect_delays_ms{500, 1000, 2000, 5000};

}  // namespace

LocalAgentClient::LocalAgentClient(QObject* parent)
    : LocalAgentClient(QStringLiteral("ai_pc_guardian_v1"), parent) {
}

LocalAgentClient::LocalAgentClient(
    QString server_name,
    QObject* parent)
    : QObject(parent),
      server_name_(std::move(server_name)) {
    reconnect_timer_.setSingleShot(true);
    poll_timer_.setInterval(poll_interval_ms);

    connect(
        &socket_,
        &QLocalSocket::connected,
        this,
        &LocalAgentClient::handle_connected);
    connect(
        &socket_,
        &QLocalSocket::disconnected,
        this,
        &LocalAgentClient::handle_disconnected);
    connect(
        &socket_,
        &QLocalSocket::readyRead,
        this,
        &LocalAgentClient::handle_ready_read);
    connect(
        &socket_,
        &QLocalSocket::errorOccurred,
        this,
        &LocalAgentClient::handle_socket_error);
    connect(
        &reconnect_timer_,
        &QTimer::timeout,
        this,
        &LocalAgentClient::connect_to_agent);
    connect(
        &poll_timer_,
        &QTimer::timeout,
        this,
        &LocalAgentClient::request_snapshot);
}

LocalAgentClient::ConnectionState
LocalAgentClient::connection_state() const noexcept {
    return connection_state_;
}

void LocalAgentClient::start() {
    if (running_) {
        return;
    }

    running_ = true;
    reconnect_attempt_ = 0;
    connect_to_agent();
}

void LocalAgentClient::stop() {
    running_ = false;
    reconnect_timer_.stop();
    poll_timer_.stop();
    receive_buffer_.clear();
    socket_.abort();
    set_connection_state(ConnectionState::Disconnected);
}

void LocalAgentClient::request_snapshot() {
    if (socket_.state() != QLocalSocket::ConnectedState) {
        return;
    }

    guardian::v1::LocalSnapshotRequest request;
    request.set_protocol_version(protocol_version);

    std::string payload;
    if (!request.SerializeToString(&payload)) {
        emit protocol_error(
            QStringLiteral("Не удалось сериализовать локальный IPC-запрос"));
        return;
    }

    const auto frame = guardian::ipc::encode_frame(payload);
    if (!frame) {
        emit protocol_error(
            QStringLiteral("Не удалось сформировать локальный IPC-frame"));
        return;
    }

    const qint64 accepted = socket_.write(
        frame->data(),
        static_cast<qint64>(frame->size()));
    if (accepted != static_cast<qint64>(frame->size())) {
        emit protocol_error(
            QStringLiteral("Не удалось поставить IPC-запрос в очередь записи"));
        socket_.abort();
    }
}

void LocalAgentClient::connect_to_agent() {
    if (!running_ ||
        socket_.state() != QLocalSocket::UnconnectedState) {
        return;
    }

    set_connection_state(ConnectionState::Connecting);
    socket_.connectToServer(server_name_, QIODevice::ReadWrite);
}

void LocalAgentClient::handle_connected() {
    reconnect_timer_.stop();
    reconnect_attempt_ = 0;
    receive_buffer_.clear();
    set_connection_state(ConnectionState::Connected);
    poll_timer_.start();
    request_snapshot();
}

void LocalAgentClient::handle_disconnected() {
    poll_timer_.stop();
    receive_buffer_.clear();
    set_connection_state(ConnectionState::Disconnected);
    schedule_reconnect();
}

void LocalAgentClient::handle_ready_read() {
    receive_buffer_.append(socket_.readAll());

    while (!receive_buffer_.isEmpty()) {
        const std::string_view bytes{
            receive_buffer_.constData(),
            static_cast<std::size_t>(receive_buffer_.size())};
        auto decoded = guardian::ipc::try_decode_frame(bytes);

        if (decoded.status == guardian::ipc::FrameDecodeStatus::Incomplete) {
            return;
        }

        if (decoded.status == guardian::ipc::FrameDecodeStatus::InvalidLength) {
            emit protocol_error(
                QStringLiteral("Agent отправил frame недопустимого размера"));
            socket_.abort();
            return;
        }

        receive_buffer_.remove(
            0,
            static_cast<qsizetype>(decoded.consumed_bytes));

        guardian::v1::LocalSnapshotResponse response;
        if (!response.ParseFromArray(
                decoded.payload.data(),
                static_cast<int>(decoded.payload.size()))) {
            emit protocol_error(
                QStringLiteral("Agent отправил повреждённый protobuf-ответ"));
            socket_.abort();
            return;
        }

        if (response.protocol_version() != protocol_version) {
            emit protocol_error(
                QStringLiteral("Версия локального протокола Agent несовместима"));
            socket_.abort();
            return;
        }

        LocalAgentSnapshot snapshot;
        switch (response.agent_state()) {
        case guardian::v1::AGENT_STATE_RUNNING:
            snapshot.state = LocalAgentState::Running;
            break;
        case guardian::v1::AGENT_STATE_DEGRADED:
            snapshot.state = LocalAgentState::Degraded;
            break;
        case guardian::v1::AGENT_STATE_STARTING:
        case guardian::v1::AGENT_STATE_UNSPECIFIED:
        default:
            snapshot.state = LocalAgentState::Starting;
            break;
        }

        snapshot.status_message = QString::fromStdString(
            response.status_message());
        snapshot.has_snapshot = response.has_snapshot();

        if (response.has_snapshot()) {
            const auto& proto_snapshot = response.snapshot();
            snapshot.collected_at_unix_ms =
                proto_snapshot.collected_at_unix_ms();

            if (proto_snapshot.has_cpu()) {
                snapshot.cpu = guardian::model::CpuMetric{
                    proto_snapshot.cpu().usage_percent()};
            }
            if (proto_snapshot.has_memory()) {
                snapshot.memory = guardian::model::MemoryMetric{
                    proto_snapshot.memory().total_bytes(),
                    proto_snapshot.memory().available_bytes()};
            }
        }

        emit snapshot_received(std::move(snapshot));
    }
}

void LocalAgentClient::handle_socket_error(
    QLocalSocket::LocalSocketError) {
    poll_timer_.stop();
    if (socket_.state() == QLocalSocket::UnconnectedState) {
        set_connection_state(ConnectionState::Disconnected);
        schedule_reconnect();
    }
}

void LocalAgentClient::schedule_reconnect() {
    if (!running_ || reconnect_timer_.isActive()) {
        return;
    }

    const int delay_index = std::min(
        reconnect_attempt_,
        static_cast<int>(reconnect_delays_ms.size()) - 1);
    reconnect_timer_.start(reconnect_delays_ms[delay_index]);

    if (reconnect_attempt_ <
        static_cast<int>(reconnect_delays_ms.size()) - 1) {
        ++reconnect_attempt_;
    }
}

void LocalAgentClient::set_connection_state(ConnectionState state) {
    if (connection_state_ == state) {
        return;
    }

    connection_state_ = state;
    emit connection_state_changed(state);
}

}  // namespace guardian::desktop
