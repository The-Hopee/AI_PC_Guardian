#include <guardian/agent/telemetry_runner.hpp>
#include <guardian/agent/windows/system_metrics_collector.hpp>
#include <guardian/tools/logger.hpp>

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>

namespace guardian::agent {

TelemetryRunner::TelemetryRunner(
    TelemetryClient& client,
    LatestSnapshotStore& snapshot_store,
    RunnerOptions options,
    guardian::model::DeviceId device_id)
    : m_client(client),
      m_snapshot_store(snapshot_store),
      m_options(std::move(options)),
      m_device_id(std::move(device_id)) {
    if (m_options.collection_interval <=
        std::chrono::milliseconds::zero()) {
        throw std::invalid_argument{"Collection interval must be positive"};
    }

    if (m_options.cpu_sample_interval <=
        std::chrono::milliseconds::zero()) {
        throw std::invalid_argument{"CPU sample interval must be positive"};
    }

    if (m_options.cpu_sample_interval > m_options.collection_interval) {
        throw std::invalid_argument{
            "CPU sample interval must not exceed collection interval"};
    }

    if (m_options.request_timeout <= std::chrono::milliseconds::zero()) {
        throw std::invalid_argument{"Request timeout must be positive"};
    }

    if (m_device_id.value.empty()) {
        throw std::invalid_argument{"Device ID must not be empty"};
    }
}

CycleResult TelemetryRunner::run_once() {
    CycleResult cycle_result{};

    const auto append_error = [](
        std::string& destination,
        std::string error) {
        if (!destination.empty()) {
            destination.append("; ");
        }

        destination.append(std::move(error));
    };

    const auto cpu_metric =
        windows::collect_cpu_metric(m_options.cpu_sample_interval);

    const auto memory_metric = windows::collect_memory_metric();

    const guardian::model::Timestamp timestamp =
        std::chrono::time_point_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now());
    const std::string timestamp_text =
        std::to_string(timestamp.time_since_epoch().count());

    LatestSnapshot snapshot{
        timestamp,
        cpu_metric,
        memory_metric,
        {},
        {}
    };

    const auto record_collection_error =
        [&append_error, &snapshot, &cycle_result](std::string error) {
            append_error(snapshot.collection_error, error);
            append_error(cycle_result.error_message, std::move(error));
        };

    const auto record_submission_error =
        [&append_error, &snapshot, &cycle_result](std::string error) {
            append_error(snapshot.submission_error, error);
            append_error(cycle_result.error_message, std::move(error));
        };

    if (!cpu_metric) {
        record_collection_error("CPU metric collection failed");
    }

    if (!memory_metric) {
        record_collection_error("Memory metric collection failed");
    }

    m_snapshot_store.update(snapshot);

    if (cpu_metric) {
        const guardian::model::EventId cpu_event_id{
            "cpu-event-" + timestamp_text};

        const guardian::model::TelemetryEvent cpu_event{
            cpu_event_id,
            timestamp,
            m_device_id,
            std::nullopt,
            *cpu_metric
        };

        const SubmitResult submit_result =
            m_client.submit(cpu_event, m_options.request_timeout);
        cycle_result.cpu_submitted = submit_result.accepted;

        if (!submit_result.accepted) {
            record_submission_error(
                "CPU telemetry submission failed: " +
                (submit_result.error_message.empty()
                     ? std::string{"unknown error"}
                     : submit_result.error_message));
        }
    }

    if (memory_metric) {
        const guardian::model::EventId memory_event_id{
            "memory-event-" + timestamp_text};

        const guardian::model::TelemetryEvent memory_event{
            memory_event_id,
            timestamp,
            m_device_id,
            std::nullopt,
            *memory_metric
        };

        const SubmitResult submit_result =
            m_client.submit(memory_event, m_options.request_timeout);
        cycle_result.memory_submitted = submit_result.accepted;

        if (!submit_result.accepted) {
            record_submission_error(
                "Memory telemetry submission failed: " +
                (submit_result.error_message.empty()
                     ? std::string{"unknown error"}
                     : submit_result.error_message));
        }
    }

    m_snapshot_store.update(std::move(snapshot));

    return cycle_result;
}

void TelemetryRunner::run(const std::atomic_bool& stop_requested) {
    using namespace std::chrono_literals;

    while (!stop_requested.load()) {
        const auto cycle_result = run_once();

        if (!cycle_result.error_message.empty()) {
            GUARDIAN_LOG_ERROR(
                "agent.telemetry",
                "Telemetry cycle completed with errors: ",
                cycle_result.error_message);
        } else {
            GUARDIAN_LOG_INFO(
                "agent.telemetry",
                "CPU and memory telemetry cycle submitted successfully");
        }

        if (stop_requested.load()) {
            break;
        }

        auto remaining = m_options.collection_interval;
        while (remaining > std::chrono::milliseconds::zero() &&
               !stop_requested.load()) {
            const auto sleep_duration = std::min(remaining, 100ms);
            std::this_thread::sleep_for(sleep_duration);
            remaining -= sleep_duration;
        }
    }
}

}  // namespace guardian::agent
