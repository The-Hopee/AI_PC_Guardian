#include <guardian/model/telemetry_event.hpp>

#include <cmath>

namespace guardian::model {

namespace {

struct EventTypeVisitor {
    EventType operator()(const CpuMetric&) const noexcept {
        return EventType::CpuMetric;
    }

    EventType operator()(const MemoryMetric&) const noexcept {
        return EventType::MemoryMetric;
    }
};

struct PayloadValidationVisitor {
    bool operator()(const CpuMetric& metric) const noexcept {
        return std::isfinite(metric.usage_percent) &&
               metric.usage_percent >= 0.0 &&
               metric.usage_percent <= 100.0;
    }

    bool operator()(const MemoryMetric& metric) const noexcept {
        return metric.total_bytes > 0 &&
               metric.available_bytes <= metric.total_bytes;
    }
};

}  // namespace

std::uint64_t MemoryMetric::used_bytes() const noexcept {
    if (available_bytes > total_bytes) {
        return 0;
    }

    return total_bytes - available_bytes;
}

EventType event_type(const EventPayload& payload) noexcept {
    return std::visit(EventTypeVisitor{}, payload);
}

bool is_valid(const TelemetryEvent& event) noexcept {
    if (event.id.value.empty() || event.device_id.value.empty()) {
        return false;
    }

    return std::visit(PayloadValidationVisitor{}, event.payload);
}

}  // namespace guardian::model
