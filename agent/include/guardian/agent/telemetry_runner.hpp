#pragma once

#include <guardian/agent/latest_snapshot_store.hpp>
#include <guardian/agent/telemetry_client.hpp>

#include <atomic>
#include <chrono>
#include <string>

namespace guardian::agent {

struct RunnerOptions {
    std::chrono::milliseconds collection_interval{5000};
    std::chrono::milliseconds cpu_sample_interval{500};
    std::chrono::milliseconds request_timeout{3000};
};

struct CycleResult {
    bool cpu_submitted{false};
    bool memory_submitted{false};
    std::string error_message;
};

class TelemetryRunner {
public:
    TelemetryRunner(
        TelemetryClient& client,
        LatestSnapshotStore& snapshot_store,
        RunnerOptions options,
        guardian::model::DeviceId device_id);

    [[nodiscard]] CycleResult run_once();
    void run(const std::atomic_bool& stop_requested);

private:
    TelemetryClient& m_client;
    LatestSnapshotStore& m_snapshot_store;
    RunnerOptions m_options;
    guardian::model::DeviceId m_device_id;
};

}  // namespace guardian::agent
