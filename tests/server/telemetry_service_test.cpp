#include <guardian/server/telemetry_service.hpp>

#include <grpcpp/support/status.h>

#include <cstdint>
#include <iostream>
#include <limits>
#include <string_view>

namespace {

int failure_count = 0;

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        ++failure_count;
    }
}

::guardian::v1::TelemetryEvent make_cpu_request(double usage_percent = 42.5) {
    ::guardian::v1::TelemetryEvent request;
    request.set_event_id("event-cpu-1");
    request.set_timestamp_unix_ms(1'725'000'123'456);
    request.set_device_id("device-7");
    request.mutable_cpu_metric()->set_usage_percent(usage_percent);
    return request;
}

::guardian::v1::TelemetryEvent make_memory_request(
    std::uint64_t total_bytes = 16'000,
    std::uint64_t available_bytes = 6'000) {
    ::guardian::v1::TelemetryEvent request;
    request.set_event_id("event-memory-1");
    request.set_timestamp_unix_ms(1'725'000'123'456);
    request.set_device_id("device-7");

    auto* metric = request.mutable_memory_metric();
    metric->set_total_bytes(total_bytes);
    metric->set_available_bytes(available_bytes);
    return request;
}

::grpc::Status submit(
    const ::guardian::v1::TelemetryEvent& request,
    ::guardian::v1::SubmitTelemetryResponse& response) {
    ::guardian::server::TelemetryServiceImpl service;
    return service.SubmitTelemetry(nullptr, &request, &response);
}

void expect_invalid_argument(
    const ::guardian::v1::TelemetryEvent& request,
    std::string_view message) {
    ::guardian::v1::SubmitTelemetryResponse response;
    const auto status = submit(request, response);

    expect(!status.ok(), message);
    expect(status.error_code() == ::grpc::StatusCode::INVALID_ARGUMENT,
           "rejected request returns INVALID_ARGUMENT");
    expect(response.event_id().empty(),
           "rejected request does not populate the response event ID");
}

void test_valid_cpu_request() {
    const auto request = make_cpu_request();
    ::guardian::v1::SubmitTelemetryResponse response;

    const auto status = submit(request, response);

    expect(status.ok(), "valid CPU telemetry is accepted");
    expect(response.event_id() == request.event_id(),
           "CPU response returns the accepted event ID");
}

void test_valid_memory_request() {
    const auto request = make_memory_request();
    ::guardian::v1::SubmitTelemetryResponse response;

    const auto status = submit(request, response);

    expect(status.ok(), "valid memory telemetry is accepted");
    expect(response.event_id() == request.event_id(),
           "memory response returns the accepted event ID");
}

void test_missing_payload_is_rejected() {
    auto request = make_cpu_request();
    request.clear_payload();
    expect_invalid_argument(request, "telemetry without payload is rejected");
}

void test_empty_identifiers_are_rejected() {
    auto empty_event_id = make_cpu_request();
    empty_event_id.clear_event_id();
    expect_invalid_argument(empty_event_id, "empty event ID is rejected");

    auto empty_device_id = make_cpu_request();
    empty_device_id.clear_device_id();
    expect_invalid_argument(empty_device_id, "empty device ID is rejected");
}

void test_invalid_metrics_are_rejected() {
    expect_invalid_argument(
        make_cpu_request(-0.1),
        "negative CPU usage is rejected");
    expect_invalid_argument(
        make_cpu_request(std::numeric_limits<double>::quiet_NaN()),
        "NaN CPU usage is rejected");
    expect_invalid_argument(
        make_memory_request(1'000, 1'001),
        "available memory above total memory is rejected");
}

void test_null_arguments_are_rejected() {
    ::guardian::server::TelemetryServiceImpl service;
    auto request = make_cpu_request();
    ::guardian::v1::SubmitTelemetryResponse response;

    const auto null_request = service.SubmitTelemetry(
        nullptr,
        nullptr,
        &response);
    expect(null_request.error_code() == ::grpc::StatusCode::INVALID_ARGUMENT,
           "null request is rejected with INVALID_ARGUMENT");

    const auto null_response = service.SubmitTelemetry(
        nullptr,
        &request,
        nullptr);
    expect(null_response.error_code() == ::grpc::StatusCode::INVALID_ARGUMENT,
           "null response is rejected with INVALID_ARGUMENT");
}

}  // namespace

int main() {
    test_valid_cpu_request();
    test_valid_memory_request();
    test_missing_payload_is_rejected();
    test_empty_identifiers_are_rejected();
    test_invalid_metrics_are_rejected();
    test_null_arguments_are_rejected();

    if (failure_count != 0) {
        std::cerr << failure_count << " test(s) failed\n";
        return 1;
    }

    return 0;
}
