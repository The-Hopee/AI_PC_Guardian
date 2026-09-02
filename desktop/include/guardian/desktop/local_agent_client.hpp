#pragma once

#include <guardian/model/metrics.hpp>

#include <QByteArray>
#include <QLocalSocket>
#include <QMetaType>
#include <QObject>
#include <QString>
#include <QTimer>

#include <cstdint>
#include <optional>

namespace guardian::desktop {

enum class LocalAgentState {
    Starting,
    Running,
    Degraded
};

struct LocalAgentSnapshot {
    LocalAgentState state{LocalAgentState::Starting};
    bool has_snapshot{false};
    std::int64_t collected_at_unix_ms{0};
    std::optional<guardian::model::CpuMetric> cpu;
    std::optional<guardian::model::MemoryMetric> memory;
    QString status_message;
};

class LocalAgentClient : public QObject {
    Q_OBJECT

public:
    enum class ConnectionState {
        Disconnected,
        Connecting,
        Connected
    };
    Q_ENUM(ConnectionState)

    explicit LocalAgentClient(QObject* parent = nullptr);
    explicit LocalAgentClient(
        QString server_name,
        QObject* parent = nullptr);

    [[nodiscard]] ConnectionState connection_state() const noexcept;

    void start();
    void stop();
    void request_snapshot();

signals:
    void connection_state_changed(
        guardian::desktop::LocalAgentClient::ConnectionState state);
    void snapshot_received(guardian::desktop::LocalAgentSnapshot snapshot);
    void protocol_error(QString message);

private:
    void connect_to_agent();
    void handle_connected();
    void handle_disconnected();
    void handle_ready_read();
    void handle_socket_error(QLocalSocket::LocalSocketError error);
    void schedule_reconnect();
    void set_connection_state(ConnectionState state);

    QString server_name_;
    QLocalSocket socket_;
    QTimer reconnect_timer_;
    QTimer poll_timer_;
    QByteArray receive_buffer_;
    ConnectionState connection_state_{ConnectionState::Disconnected};
    int reconnect_attempt_{0};
    bool running_{false};
};

}  // namespace guardian::desktop

Q_DECLARE_METATYPE(guardian::desktop::LocalAgentSnapshot)
