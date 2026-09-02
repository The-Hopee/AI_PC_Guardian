#include <guardian/v1/local_agent.pb.h>

#include <cstdint>
#include <iostream>
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

}  // namespace

int main() {
    guardian::v1::LocalSnapshotRequest request;
    request.set_protocol_version(1);

    std::string request_bytes;
    expect(request.SerializeToString(&request_bytes),
           "snapshot request serializes");

    guardian::v1::LocalSnapshotRequest parsed_request;
    expect(parsed_request.ParseFromString(request_bytes),
           "snapshot request parses");
    expect(parsed_request.protocol_version() == 1,
           "snapshot request preserves protocol version");

    guardian::v1::LocalSnapshotResponse response;
    response.set_protocol_version(1);
    response.set_agent_state(guardian::v1::AGENT_STATE_DEGRADED);
    response.set_status_message("remote server unavailable");

    auto* snapshot = response.mutable_snapshot();
    snapshot->set_collected_at_unix_ms(1'725'000'000'123);
    snapshot->mutable_cpu()->set_usage_percent(37.5);
    snapshot->mutable_memory()->set_total_bytes(
        std::uint64_t{16} * 1024 * 1024 * 1024);
    snapshot->mutable_memory()->set_available_bytes(
        std::uint64_t{6} * 1024 * 1024 * 1024);

    std::string response_bytes;
    expect(response.SerializeToString(&response_bytes),
           "snapshot response serializes");

    guardian::v1::LocalSnapshotResponse parsed_response;
    expect(parsed_response.ParseFromString(response_bytes),
           "snapshot response parses");
    expect(parsed_response.protocol_version() == 1,
           "snapshot response preserves protocol version");
    expect(parsed_response.agent_state() ==
               guardian::v1::AGENT_STATE_DEGRADED,
           "snapshot response preserves agent state");
    expect(parsed_response.status_message() ==
               "remote server unavailable",
           "snapshot response preserves status message");
    expect(parsed_response.has_snapshot(),
           "snapshot response reports message presence");

    if (parsed_response.has_snapshot()) {
        const auto& parsed_snapshot = parsed_response.snapshot();
        expect(parsed_snapshot.collected_at_unix_ms() ==
                   1'725'000'000'123,
               "metric snapshot preserves timestamp");
        expect(parsed_snapshot.has_cpu(),
               "metric snapshot reports CPU presence");
        expect(parsed_snapshot.has_memory(),
               "metric snapshot reports memory presence");
        expect(parsed_snapshot.cpu().usage_percent() == 37.5,
               "metric snapshot preserves CPU usage");
        expect(parsed_snapshot.memory().total_bytes() ==
                   std::uint64_t{16} * 1024 * 1024 * 1024,
               "metric snapshot preserves total memory");
        expect(parsed_snapshot.memory().available_bytes() ==
                   std::uint64_t{6} * 1024 * 1024 * 1024,
               "metric snapshot preserves available memory");
    }

    guardian::v1::LocalSnapshotResponse starting_response;
    starting_response.set_protocol_version(1);
    starting_response.set_agent_state(guardian::v1::AGENT_STATE_STARTING);
    expect(!starting_response.has_snapshot(),
           "starting response can omit the entire snapshot");

    guardian::v1::MetricSnapshot partial_snapshot;
    partial_snapshot.mutable_memory()->set_total_bytes(4096);
    partial_snapshot.mutable_memory()->set_available_bytes(1024);
    expect(!partial_snapshot.has_cpu(),
           "partial snapshot can omit CPU metric");
    expect(partial_snapshot.has_memory(),
           "partial snapshot can contain only memory metric");

    if (failure_count != 0) {
        std::cerr << failure_count << " test(s) failed\n";
        return 1;
    }

    return 0;
}
