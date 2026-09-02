#include <guardian/agent/telemetry_runner.hpp>
#include <guardian/v1/telemetry_service.grpc.pb.h>

#include <grpcpp/grpcpp.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

int failure_count = 0;

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        ++failure_count;
    }
}

class RecordingTelemetryService final
    : public guardian::v1::TelemetryService::Service {
public:
    grpc::Status SubmitTelemetry(
        grpc::ServerContext*,
        const guardian::v1::TelemetryEvent* request,
        guardian::v1::SubmitTelemetryResponse* response) override {
        {
            const std::lock_guard<std::mutex> lock{mutex_};
            requests_.push_back(*request);
        }
        ++call_count;

        if (reject_cpu.load() && request->has_cpu_metric()) {
            return {
                grpc::StatusCode::UNAVAILABLE,
                "CPU rejected by test server"};
        }

        response->set_event_id(request->event_id());
        return grpc::Status::OK;
    }

    std::vector<guardian::v1::TelemetryEvent> requests() const {
        const std::lock_guard<std::mutex> lock{mutex_};
        return requests_;
    }

    std::atomic_bool reject_cpu{false};
    std::atomic_int call_count{0};

private:
    mutable std::mutex mutex_;
    std::vector<guardian::v1::TelemetryEvent> requests_;
};

template <typename Function>
bool throws_invalid_argument(Function&& function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }

    return false;
}

}  // namespace

int main() {
    RecordingTelemetryService service;
    grpc::ServerBuilder builder;

    int selected_port = 0;
    builder.AddListeningPort(
        "127.0.0.1:0",
        grpc::InsecureServerCredentials(),
        &selected_port);
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server = builder.BuildAndStart();
    expect(server != nullptr, "loopback server starts");
    expect(selected_port > 0, "loopback server receives a free port");
    if (!server || selected_port <= 0) {
        return 1;
    }

    const auto channel = grpc::CreateChannel(
        "127.0.0.1:" + std::to_string(selected_port),
        grpc::InsecureChannelCredentials());
    guardian::agent::TelemetryClient client{channel};
    const guardian::agent::RunnerOptions options{
        std::chrono::milliseconds{200},
        std::chrono::milliseconds{20},
        std::chrono::seconds{2},
    };
    guardian::agent::LatestSnapshotStore snapshot_store;
    guardian::agent::TelemetryRunner runner{
        client,
        snapshot_store,
        options,
        guardian::model::DeviceId{"runner-test-device"}};

    const auto successful = runner.run_once();
    expect(successful.cpu_submitted, "runner submits CPU telemetry");
    expect(successful.memory_submitted, "runner submits memory telemetry");
    expect(successful.error_message.empty(),
           "successful cycle has no error message");

    const auto successful_snapshot = snapshot_store.get();
    expect(successful_snapshot.has_value(),
           "successful cycle publishes a local snapshot");
    if (successful_snapshot) {
        expect(successful_snapshot->cpu.has_value(),
               "successful snapshot contains CPU metric");
        expect(successful_snapshot->memory.has_value(),
               "successful snapshot contains memory metric");
        expect(successful_snapshot->collection_error.empty(),
               "successful snapshot has no collection error");
        expect(successful_snapshot->submission_error.empty(),
               "successful snapshot has no submission error");
    }

    const auto first_requests = service.requests();
    expect(first_requests.size() == 2, "successful cycle performs two RPCs");
    if (first_requests.size() == 2) {
        expect(first_requests[0].has_cpu_metric(),
               "first event contains CPU payload");
        expect(first_requests[1].has_memory_metric(),
               "second event contains memory payload");
        expect(first_requests[0].device_id() == "runner-test-device" &&
                   first_requests[1].device_id() == "runner-test-device",
               "both events use the runner device ID");
        expect(first_requests[0].event_id() != first_requests[1].event_id(),
               "CPU and memory event IDs differ");
    }

    service.reject_cpu = true;
    const auto partial = runner.run_once();
    expect(!partial.cpu_submitted, "CPU server error is reported");
    expect(partial.memory_submitted,
           "CPU server error does not prevent memory submission");
    expect(partial.error_message.find("CPU rejected by test server") !=
               std::string::npos,
           "runner preserves the CPU submission error");

    const auto partial_snapshot = snapshot_store.get();
    expect(partial_snapshot.has_value(),
           "partially submitted cycle replaces the local snapshot");
    if (partial_snapshot) {
        expect(partial_snapshot->cpu.has_value() &&
                   partial_snapshot->memory.has_value(),
               "submission failure does not remove collected metrics");
        expect(partial_snapshot->collection_error.empty(),
               "submission failure is not reported as collection failure");
        expect(partial_snapshot->submission_error.find(
                   "CPU rejected by test server") != std::string::npos,
               "snapshot preserves the CPU submission error");
    }

    auto zero_collection = options;
    zero_collection.collection_interval = std::chrono::milliseconds::zero();
    expect(throws_invalid_argument([&] {
        guardian::agent::TelemetryRunner invalid{
            client,
            snapshot_store,
            zero_collection,
            guardian::model::DeviceId{"device"}};
    }), "zero collection interval is rejected");

    auto excessive_sample = options;
    excessive_sample.cpu_sample_interval = std::chrono::milliseconds{201};
    expect(throws_invalid_argument([&] {
        guardian::agent::TelemetryRunner invalid{
            client,
            snapshot_store,
            excessive_sample,
            guardian::model::DeviceId{"device"}};
    }), "CPU sample interval above collection interval is rejected");

    expect(throws_invalid_argument([&] {
        guardian::agent::TelemetryRunner invalid{
            client,
            snapshot_store,
            options,
            guardian::model::DeviceId{}};
    }), "empty device ID is rejected");

    service.reject_cpu = false;
    std::atomic_bool stop{false};
    const int calls_before_run = service.call_count.load();
    std::thread runner_thread{[&] { runner.run(stop); }};

    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds{2};
    while (service.call_count.load() < calls_before_run + 2 &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }

    const auto stop_started = std::chrono::steady_clock::now();
    stop = true;
    runner_thread.join();
    const auto stop_elapsed =
        std::chrono::steady_clock::now() - stop_started;
    expect(service.call_count.load() >= calls_before_run + 2,
           "continuous runner performs a telemetry cycle");
    expect(stop_elapsed < std::chrono::milliseconds{500},
           "continuous runner reacts promptly to the stop flag");

    server->Shutdown();
    server->Wait();

    if (failure_count != 0) {
        std::cerr << failure_count << " test(s) failed\n";
        return 1;
    }

    return 0;
}
