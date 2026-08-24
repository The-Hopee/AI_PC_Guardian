#include <guardian/server/telemetry_service.hpp>
#include <guardian/v1/telemetry_service.grpc.pb.h>

#include <grpcpp/grpcpp.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

namespace {

int failure_count = 0;

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        ++failure_count;
    }
}

::guardian::v1::TelemetryEvent make_cpu_request() {
    ::guardian::v1::TelemetryEvent request;
    request.set_event_id("network-event-1");
    request.set_timestamp_unix_ms(1'725'000'123'456);
    request.set_device_id("network-device-1");
    request.mutable_cpu_metric()->set_usage_percent(51.25);
    return request;
}

::grpc::Status submit(
    ::guardian::v1::TelemetryService::Stub& stub,
    const ::guardian::v1::TelemetryEvent& request,
    ::guardian::v1::SubmitTelemetryResponse& response) {
    ::grpc::ClientContext context;
    context.set_deadline(
        std::chrono::system_clock::now() + std::chrono::seconds{5});
    return stub.SubmitTelemetry(&context, request, &response);
}

}  // namespace

int main() {
    ::guardian::server::TelemetryServiceImpl service;
    ::grpc::ServerBuilder builder;

    int selected_port = 0;
    builder.AddListeningPort(
        "127.0.0.1:0",
        ::grpc::InsecureServerCredentials(),
        &selected_port);
    builder.RegisterService(&service);

    std::unique_ptr<::grpc::Server> server = builder.BuildAndStart();
    expect(server != nullptr, "loopback gRPC server starts");
    expect(selected_port > 0, "gRPC chooses a free loopback port");

    if (!server || selected_port <= 0) {
        return 1;
    }

    const std::string address =
        "127.0.0.1:" + std::to_string(selected_port);
    const auto channel = ::grpc::CreateChannel(
        address,
        ::grpc::InsecureChannelCredentials());
    auto stub = ::guardian::v1::TelemetryService::NewStub(channel);

    const auto valid_request = make_cpu_request();
    ::guardian::v1::SubmitTelemetryResponse valid_response;
    const auto valid_status = submit(*stub, valid_request, valid_response);

    expect(valid_status.ok(), "valid telemetry passes through the gRPC network stack");
    expect(valid_response.event_id() == valid_request.event_id(),
           "network response contains the accepted event ID");

    auto invalid_request = make_cpu_request();
    invalid_request.clear_payload();
    ::guardian::v1::SubmitTelemetryResponse invalid_response;
    const auto invalid_status = submit(*stub, invalid_request, invalid_response);

    expect(invalid_status.error_code() == ::grpc::StatusCode::INVALID_ARGUMENT,
           "invalid network request returns INVALID_ARGUMENT");

    server->Shutdown();
    server->Wait();

    if (failure_count != 0) {
        std::cerr << failure_count << " test(s) failed\n";
        return 1;
    }

    return 0;
}
