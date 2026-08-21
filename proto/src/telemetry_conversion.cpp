#include <guardian/proto/telemetry_conversion.hpp>

#include <chrono>

namespace guardian::proto {

namespace {

struct ToProtoPayloadVisitor {
    ::guardian::v1::TelemetryEvent& message;

    void operator()(const ::guardian::model::CpuMetric& metric) const {
        message.mutable_cpu_metric()->set_usage_percent(
            metric.usage_percent);
    }

    void operator()(const ::guardian::model::MemoryMetric& metric) const {
        auto* proto_metric = message.mutable_memory_metric();
        proto_metric->set_total_bytes(metric.total_bytes);
        proto_metric->set_available_bytes(metric.available_bytes);
    }
};

}  // namespace

std::optional<::guardian::v1::TelemetryEvent> to_proto(
    const ::guardian::model::TelemetryEvent& event) {
    if (!::guardian::model::is_valid(event)) {
        return std::nullopt;
    }

    ::guardian::v1::TelemetryEvent message;
    message.set_event_id(event.id.value);
    message.set_device_id(event.device_id.value);
    message.set_timestamp_unix_ms(event.timestamp.time_since_epoch().count());

    if (event.process_id.has_value()) {
        message.set_process_id(event.process_id->value);
    }

    std::visit(ToProtoPayloadVisitor{message}, event.payload);

    return message;
}

std::optional<::guardian::model::TelemetryEvent> from_proto(
    const ::guardian::v1::TelemetryEvent& message) {
    std::optional<::guardian::model::EventPayload> payload;

    switch (message.payload_case()) {
        case ::guardian::v1::TelemetryEvent::kCpuMetric:
            payload = ::guardian::model::CpuMetric{
                message.cpu_metric().usage_percent()
            };
            break;
        case ::guardian::v1::TelemetryEvent::kMemoryMetric:
            payload = ::guardian::model::MemoryMetric{
                message.memory_metric().total_bytes(),
                message.memory_metric().available_bytes()
            };
            break;
        case ::guardian::v1::TelemetryEvent::PAYLOAD_NOT_SET:
        default:
            return std::nullopt;
    }

    std::optional<::guardian::model::ProcessId> process_id;
    if (message.has_process_id()) {
        process_id = ::guardian::model::ProcessId{message.process_id()};
    }

    const ::guardian::model::TelemetryEvent event{
        ::guardian::model::EventId{message.event_id()},
        ::guardian::model::Timestamp{
            std::chrono::milliseconds{message.timestamp_unix_ms()}},
        ::guardian::model::DeviceId{message.device_id()},
        process_id,
        *payload,
    };

    if (!::guardian::model::is_valid(event)) {
        return std::nullopt;
    }

    return event;
}

}  // namespace guardian::proto
