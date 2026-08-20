#include <guardian/model/telemetry_event.hpp>

#include <iostream>
#include <limits>
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

guardian::model::TelemetryEvent make_event(
    guardian::model::EventPayload payload) {
    using namespace guardian::model;

    return {
        EventId{"event-1"},
        Timestamp{},
        DeviceId{"device-1"},
        std::nullopt,
        std::move(payload),
    };
}

}  // namespace

int main() {
    using namespace guardian::model;

    expect(MemoryMetric{16, 6}.used_bytes() == 10,
           "used memory is total minus available");
    expect(MemoryMetric{16, 16}.used_bytes() == 0,
           "fully available memory has zero used bytes");
    expect(MemoryMetric{16, 17}.used_bytes() == 0,
           "invalid memory values do not underflow");

    expect(event_type(EventPayload{CpuMetric{50.0}}) == EventType::CpuMetric,
           "CPU payload maps to CpuMetric event type");
    expect(event_type(EventPayload{MemoryMetric{16, 6}}) == EventType::MemoryMetric,
           "memory payload maps to MemoryMetric event type");

    expect(is_valid(make_event(CpuMetric{0.0})),
           "zero CPU usage is valid");
    expect(is_valid(make_event(CpuMetric{100.0})),
           "100 percent CPU usage is valid");
    expect(!is_valid(make_event(CpuMetric{-0.1})),
           "negative CPU usage is invalid");
    expect(!is_valid(make_event(CpuMetric{100.1})),
           "CPU usage over 100 percent is invalid");
    expect(!is_valid(make_event(CpuMetric{
               std::numeric_limits<double>::quiet_NaN()})),
           "NaN CPU usage is invalid");
    expect(!is_valid(make_event(CpuMetric{
               std::numeric_limits<double>::infinity()})),
           "infinite CPU usage is invalid");

    expect(is_valid(make_event(MemoryMetric{16, 6})),
           "consistent memory values are valid");
    expect(!is_valid(make_event(MemoryMetric{0, 0})),
           "zero total memory is invalid");
    expect(!is_valid(make_event(MemoryMetric{16, 17})),
           "available memory cannot exceed total memory");

    auto empty_event_id = make_event(CpuMetric{50.0});
    empty_event_id.id.value.clear();
    expect(!is_valid(empty_event_id), "empty event ID is invalid");

    auto empty_device_id = make_event(CpuMetric{50.0});
    empty_device_id.device_id.value.clear();
    expect(!is_valid(empty_device_id), "empty device ID is invalid");

    if (failure_count != 0) {
        std::cerr << failure_count << " test(s) failed\n";
        return 1;
    }

    return 0;
}
