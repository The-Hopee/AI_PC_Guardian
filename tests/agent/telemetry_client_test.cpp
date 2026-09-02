#include <guardian/agent/telemetry_client.hpp>
#include <guardian/model/telemetry_event.hpp>
#include <guardian/v1/telemetry_service.grpc.pb.h>

#include <grpcpp/grpcpp.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <optional>
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

guardian::model::TelemetryEvent make_valid_event() {
    return {
        guardian::model::EventId{"agent-event-1"},
        guardian::model::Timestamp{std::chrono::milliseconds{1'725'000'123'456}},
        guardian::model::DeviceId{"agent-device-1"},
        std::nullopt,
        guardian::model::CpuMetric{37.5},
    };
}

class TestTelemetryService final
    : public guardian::v1::TelemetryService::Service {
public:
    enum class Mode {
        accept,
        reject,
        empty_response,
    };

    grpc::Status SubmitTelemetry(
        grpc::ServerContext*,
        const guardian::v1::TelemetryEvent* request,
        guardian::v1::SubmitTelemetryResponse* response) override {
        ++call_count;

        switch (mode.load()) {
        case Mode::accept:
            response->set_event_id(request->event_id());
            return grpc::Status::OK;
        case Mode::reject:
            return {grpc::StatusCode::INVALID_ARGUMENT, "Rejected by test server"};
        case Mode::empty_response:
            return grpc::Status::OK;
        }

        return {grpc::StatusCode::INTERNAL, "Unknown test mode"};
    }

    std::atomic<Mode> mode{Mode::accept};
    std::atomic<int> call_count{0};
};

}  // namespace

int main() {
    TestTelemetryService service;
    grpc::ServerBuilder builder;

    int selected_port = 0;
    builder.AddListeningPort(
        "127.0.0.1:0",
        grpc::InsecureServerCredentials(),
        &selected_port);
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server = builder.BuildAndStart();
    expect(server != nullptr, "loopback test server starts");
    expect(selected_port > 0, "loopback test server receives a free port");

    if (!server || selected_port <= 0) {
        return 1;
    }

    const std::string address =
        "127.0.0.1:" + std::to_string(selected_port);
    const auto channel = grpc::CreateChannel(
        address,
        grpc::InsecureChannelCredentials());
    guardian::agent::TelemetryClient client{channel};

    const auto valid_event = make_valid_event();
    const auto accepted = client.submit(valid_event, std::chrono::seconds{2});
    expect(accepted.accepted, "valid event is accepted through a real gRPC channel");
    expect(accepted.event_id == valid_event.id.value,
           "client returns the event ID from the server");
    expect(accepted.error_message.empty(), "successful result has no error message");

    const int calls_before_invalid_event = service.call_count.load();
    auto invalid_event = valid_event;
    invalid_event.id.value.clear();
    const auto invalid = client.submit(invalid_event, std::chrono::seconds{2});
    expect(!invalid.accepted, "invalid domain event is rejected locally");
    expect(!invalid.error_message.empty(), "local validation returns an error message");
    expect(service.call_count.load() == calls_before_invalid_event,
           "invalid domain event does not perform an RPC");

    service.mode = TestTelemetryService::Mode::reject;
    const auto rejected = client.submit(valid_event, std::chrono::seconds{2});
    expect(!rejected.accepted, "server gRPC error produces a failed result");
    expect(rejected.event_id.empty(), "failed result does not contain an event ID");
    expect(rejected.error_message == "Rejected by test server",
           "client preserves the safe server error message");

    service.mode = TestTelemetryService::Mode::empty_response;
    const auto empty_response = client.submit(valid_event, std::chrono::seconds{2});
    expect(!empty_response.accepted, "empty server event ID is rejected");
    expect(!empty_response.error_message.empty(),
           "empty server event ID returns an error message");

    server->Shutdown();
    server->Wait();

    const auto unavailable =
        client.submit(valid_event, std::chrono::milliseconds{200});
    expect(!unavailable.accepted, "unavailable server produces a failed result");
    expect(!unavailable.error_message.empty(),
           "unavailable server returns an error message");

    if (failure_count != 0) {
        std::cerr << failure_count << " test(s) failed\n";
        return 1;
    }

    return 0;
}
