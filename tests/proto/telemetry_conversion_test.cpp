#include <guardian/proto/telemetry_conversion.hpp>

#include <chrono>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace {

int failure_count = 0;

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        ++failure_count;
    }
}

guardian::model::TelemetryEvent make_event(
    guardian::model::EventPayload payload,
    std::optional<guardian::model::ProcessId> process_id = std::nullopt) {
    using namespace guardian::model;

    return {
        EventId{"event-42"},
        Timestamp{std::chrono::milliseconds{1'725'000'123'456}},
        DeviceId{"device-7"},
        process_id,
        std::move(payload),
    };
}

void expect_metadata_equal(
    const guardian::model::TelemetryEvent& expected,
    const guardian::model::TelemetryEvent& actual) {
    expect(actual.id.value == expected.id.value,
           "event ID survives conversion");
    expect(actual.device_id.value == expected.device_id.value,
           "device ID survives conversion");
    expect(actual.timestamp == expected.timestamp,
           "timestamp survives conversion");
    expect(actual.process_id.has_value() == expected.process_id.has_value(),
           "process ID presence survives conversion");

    if (actual.process_id.has_value() && expected.process_id.has_value()) {
        expect(actual.process_id->value == expected.process_id->value,
               "process ID value survives conversion");
    }
}

void test_cpu_round_trip() {
    using namespace guardian::model;

    const auto source = make_event(CpuMetric{37.5}, ProcessId{4242});
    const auto message = guardian::proto::to_proto(source);

    expect(message.has_value(), "valid CPU event converts to Protobuf");
    if (!message.has_value()) {
        return;
    }

    expect(message->payload_case() ==
               guardian::v1::TelemetryEvent::kCpuMetric,
           "CPU payload selects Protobuf CPU oneof case");
    expect(message->cpu_metric().usage_percent() == 37.5,
           "CPU value is written to Protobuf");
    expect(message->has_process_id(),
           "present process ID is written to Protobuf");

    const auto restored = guardian::proto::from_proto(*message);
    expect(restored.has_value(), "valid CPU Protobuf message converts back");
    if (!restored.has_value()) {
        return;
    }

    expect_metadata_equal(source, *restored);
    expect(std::holds_alternative<CpuMetric>(restored->payload),
           "CPU payload returns as model CPU variant");
    expect(std::get<CpuMetric>(restored->payload).usage_percent == 37.5,
           "CPU value survives round trip");
}

void test_memory_round_trip() {
    using namespace guardian::model;

    const auto source = make_event(MemoryMetric{16'000, 6'000});
    const auto message = guardian::proto::to_proto(source);

    expect(message.has_value(), "valid memory event converts to Protobuf");
    if (!message.has_value()) {
        return;
    }

    expect(message->payload_case() ==
               guardian::v1::TelemetryEvent::kMemoryMetric,
           "memory payload selects Protobuf memory oneof case");
    expect(message->memory_metric().total_bytes() == 16'000,
           "total memory is written to Protobuf");
    expect(message->memory_metric().available_bytes() == 6'000,
           "available memory is written to Protobuf");
    expect(!message->has_process_id(),
           "absent process ID stays absent in Protobuf");

    const auto restored = guardian::proto::from_proto(*message);
    expect(restored.has_value(),
           "valid memory Protobuf message converts back");
    if (!restored.has_value()) {
        return;
    }

    expect_metadata_equal(source, *restored);
    expect(std::holds_alternative<MemoryMetric>(restored->payload),
           "memory payload returns as model memory variant");

    const auto& metric = std::get<MemoryMetric>(restored->payload);
    expect(metric.total_bytes == 16'000,
           "total memory survives round trip");
    expect(metric.available_bytes == 6'000,
           "available memory survives round trip");
}

void test_invalid_model_events_are_rejected() {
    using namespace guardian::model;

    auto empty_event_id = make_event(CpuMetric{50.0});
    empty_event_id.id.value.clear();
    expect(!guardian::proto::to_proto(empty_event_id).has_value(),
           "empty model event ID is rejected");

    auto empty_device_id = make_event(CpuMetric{50.0});
    empty_device_id.device_id.value.clear();
    expect(!guardian::proto::to_proto(empty_device_id).has_value(),
           "empty model device ID is rejected");

    expect(!guardian::proto::to_proto(make_event(CpuMetric{-0.1})).has_value(),
           "invalid model CPU value is rejected");
    expect(!guardian::proto::to_proto(
               make_event(MemoryMetric{1'000, 1'001})).has_value(),
           "invalid model memory value is rejected");
}

guardian::v1::TelemetryEvent make_proto_cpu(double usage_percent) {
    guardian::v1::TelemetryEvent message;
    message.set_event_id("event-42");
    message.set_timestamp_unix_ms(1'725'000'123'456);
    message.set_device_id("device-7");
    message.mutable_cpu_metric()->set_usage_percent(usage_percent);
    return message;
}

void test_invalid_proto_messages_are_rejected() {
    guardian::v1::TelemetryEvent missing_payload;
    missing_payload.set_event_id("event-42");
    missing_payload.set_device_id("device-7");
    expect(!guardian::proto::from_proto(missing_payload).has_value(),
           "Protobuf message without payload is rejected");

    auto empty_event_id = make_proto_cpu(50.0);
    empty_event_id.clear_event_id();
    expect(!guardian::proto::from_proto(empty_event_id).has_value(),
           "empty Protobuf event ID is rejected");

    auto empty_device_id = make_proto_cpu(50.0);
    empty_device_id.clear_device_id();
    expect(!guardian::proto::from_proto(empty_device_id).has_value(),
           "empty Protobuf device ID is rejected");

    expect(!guardian::proto::from_proto(make_proto_cpu(
               std::numeric_limits<double>::quiet_NaN())).has_value(),
           "invalid Protobuf CPU value is rejected");

    guardian::v1::TelemetryEvent invalid_memory;
    invalid_memory.set_event_id("event-42");
    invalid_memory.set_device_id("device-7");
    auto* memory = invalid_memory.mutable_memory_metric();
    memory->set_total_bytes(1'000);
    memory->set_available_bytes(1'001);
    expect(!guardian::proto::from_proto(invalid_memory).has_value(),
           "invalid Protobuf memory value is rejected");
}

void test_binary_serialization_round_trip() {
    using namespace guardian::model;

    const auto source = make_event(CpuMetric{82.25}, ProcessId{17});
    const auto message = guardian::proto::to_proto(source);
    expect(message.has_value(), "event for serialization converts");
    if (!message.has_value()) {
        return;
    }

    std::string bytes;
    expect(message->SerializeToString(&bytes),
           "Protobuf message serializes to bytes");

    guardian::v1::TelemetryEvent parsed;
    expect(parsed.ParseFromString(bytes),
           "serialized bytes parse into a new Protobuf message");

    const auto restored = guardian::proto::from_proto(parsed);
    expect(restored.has_value(), "parsed message converts to model");
    if (!restored.has_value()) {
        return;
    }

    expect_metadata_equal(source, *restored);
    expect(std::get<CpuMetric>(restored->payload).usage_percent == 82.25,
           "payload survives binary serialization");
}

}  // namespace

int main() {
    test_cpu_round_trip();
    test_memory_round_trip();
    test_invalid_model_events_are_rejected();
    test_invalid_proto_messages_are_rejected();
    test_binary_serialization_round_trip();

    if (failure_count != 0) {
        std::cerr << failure_count << " test(s) failed\n";
        return 1;
    }

    return 0;
}
