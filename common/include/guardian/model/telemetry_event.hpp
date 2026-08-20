#pragma once

#include <guardian/model/identifiers.hpp>
#include <guardian/model/metrics.hpp>

#include <optional>
#include <variant>

namespace guardian::model {

enum class EventType {
    CpuMetric,
    MemoryMetric
};

using EventPayload = std::variant<CpuMetric, MemoryMetric>;

struct TelemetryEvent {
    EventId id;
    Timestamp timestamp;
    DeviceId device_id;
    std::optional<ProcessId> process_id;
    EventPayload payload;
};

[[nodiscard]]
EventType event_type(const EventPayload& payload) noexcept;

[[nodiscard]]
bool is_valid(const TelemetryEvent& event) noexcept;

}  // namespace guardian::model
