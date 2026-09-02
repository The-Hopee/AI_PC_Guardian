#include <guardian/agent/telemetry_client.hpp>
#include <guardian/proto/telemetry_conversion.hpp>

#include <utility>

namespace guardian::agent {

TelemetryClient::TelemetryClient(std::shared_ptr<grpc::Channel> channel)
    : stub_(guardian::v1::TelemetryService::NewStub(std::move(channel))) {}

SubmitResult TelemetryClient::submit(
    const guardian::model::TelemetryEvent& event,
    std::chrono::milliseconds timeout) {
    const auto proto_event = guardian::proto::to_proto(event);
    if (!proto_event) {
        return {false, {}, "Invalid telemetry event"};
    }

    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + timeout);

    guardian::v1::SubmitTelemetryResponse response;
    const grpc::Status status =
        stub_->SubmitTelemetry(&context, *proto_event, &response);

    if (!status.ok()) {
        const std::string error_message = status.error_message().empty()
            ? "Telemetry RPC failed"
            : status.error_message();
        return {false, {}, error_message};
    }

    if (response.event_id().empty()) {
        return {false, {}, "Server returned an empty event ID"};
    }

    return {true, response.event_id(), {}};
}

}  // namespace guardian::agent
