#pragma once

#include <guardian/desktop/local_agent_client.hpp>

#include <QObject>
#include <QString>
#include <QtQmlIntegration>

namespace guardian::desktop {

class SystemMetricsViewModel : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(SystemMetrics)

    Q_PROPERTY(double cpuUsage READ cpu_usage NOTIFY metrics_changed)
    Q_PROPERTY(double memoryUsage READ memory_usage NOTIFY metrics_changed)
    Q_PROPERTY(double memoryUsedGiB READ memory_used_gib NOTIFY metrics_changed)
    Q_PROPERTY(double memoryTotalGiB READ memory_total_gib NOTIFY metrics_changed)
    Q_PROPERTY(QString statusText READ status_text NOTIFY state_changed)
    Q_PROPERTY(QString lastUpdated READ last_updated NOTIFY state_changed)
    Q_PROPERTY(bool refreshing READ refreshing NOTIFY state_changed)
    Q_PROPERTY(bool agentAvailable READ agent_available NOTIFY state_changed)
    Q_PROPERTY(bool dataStale READ data_stale NOTIFY state_changed)

public:
    explicit SystemMetricsViewModel(QObject* parent = nullptr);
    explicit SystemMetricsViewModel(
        QString agent_server_name,
        QObject* parent = nullptr);
    ~SystemMetricsViewModel() override;

    [[nodiscard]] double cpu_usage() const noexcept;
    [[nodiscard]] double memory_usage() const noexcept;
    [[nodiscard]] double memory_used_gib() const noexcept;
    [[nodiscard]] double memory_total_gib() const noexcept;
    [[nodiscard]] QString status_text() const;
    [[nodiscard]] QString last_updated() const;
    [[nodiscard]] bool refreshing() const noexcept;
    [[nodiscard]] bool agent_available() const noexcept;
    [[nodiscard]] bool data_stale() const noexcept;

    Q_INVOKABLE void refresh();

signals:
    void metrics_changed();
    void state_changed();

private:
    void apply_snapshot(LocalAgentSnapshot snapshot);
    void apply_connection_state(LocalAgentClient::ConnectionState state);

    LocalAgentClient client_;
    double cpu_usage_{0.0};
    double memory_usage_{0.0};
    double memory_used_gib_{0.0};
    double memory_total_gib_{0.0};
    QString status_text_{QStringLiteral("Ожидание первого измерения")};
    QString last_updated_{QStringLiteral("—")};
    bool refreshing_{false};
    bool agent_available_{false};
    bool data_stale_{false};
    bool has_snapshot_{false};
};

}  // namespace guardian::desktop
