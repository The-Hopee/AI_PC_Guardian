#pragma once

#include <guardian/v1/telemetry_service.grpc.pb.h>

namespace guardian::server {

class TelemetryServiceImpl final
    : public ::guardian::v1::TelemetryService::Service {
public:
    ::grpc::Status SubmitTelemetry(
        ::grpc::ServerContext* context,
        const ::guardian::v1::TelemetryEvent* request,
        ::guardian::v1::SubmitTelemetryResponse* response) override;
};

}  // namespace guardian::server
