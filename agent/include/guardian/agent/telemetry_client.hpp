#pragma once

#include <guardian/model/telemetry_event.hpp>
#include <guardian/v1/telemetry_service.grpc.pb.h>

#include <grpcpp/channel.h>

#include <chrono>
#include <memory>
#include <string>

namespace guardian::agent {

struct SubmitResult {
    bool accepted{false};
    std::string event_id;
    std::string error_message;
};

class TelemetryClient {
public:
    explicit TelemetryClient(std::shared_ptr<grpc::Channel> channel);

    [[nodiscard]]
    SubmitResult submit(
        const guardian::model::TelemetryEvent& event,
        std::chrono::milliseconds timeout);

private:
    std::unique_ptr<guardian::v1::TelemetryService::Stub> stub_;
};

}  // namespace guardian::agent