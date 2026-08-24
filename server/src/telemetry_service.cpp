#include <guardian/proto/telemetry_conversion.hpp>
#include <guardian/server/telemetry_service.hpp>

#include <iostream>

namespace guardian::server {

::grpc::Status TelemetryServiceImpl::SubmitTelemetry(
    ::grpc::ServerContext* context,
    const ::guardian::v1::TelemetryEvent* request,
    ::guardian::v1::SubmitTelemetryResponse* response) {
    (void)context;

    if (request == nullptr || response == nullptr) {
        return {
            ::grpc::StatusCode::INVALID_ARGUMENT,
            "Request and response must not be null"};
    }

    const auto event = ::guardian::proto::from_proto(*request);
    if (!event) {
        return {
            ::grpc::StatusCode::INVALID_ARGUMENT,
            "Invalid telemetry event"};
    }

    response->set_event_id(event->id.value);
    std::clog << "Accepted telemetry event " << event->id.value << '\n';
    return ::grpc::Status::OK;
}

}  // namespace guardian::server
