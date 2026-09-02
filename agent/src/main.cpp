#include <guardian/agent/local_ipc_server.hpp>
#include <guardian/agent/telemetry_runner.hpp>
#include <guardian/tools/logger.hpp>
#include <guardian/version.hpp>

#include <grpcpp/grpcpp.h>

#include <atomic>
#include <charconv>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>

namespace {

std::atomic_bool stop_requested{false};

void request_stop(int) noexcept {
    stop_requested.store(true);
}

std::string usage_text(std::string_view program_name) {
    std::ostringstream output;
    output << "Usage: " << program_name
           << " [--server HOST:PORT] [--interval-ms N] [--once]\n"
           << "\n"
           << "Options:\n"
           << "  --server HOST:PORT  gRPC server address"
              " (default: 127.0.0.1:50051)\n"
           << "  --interval-ms N     Collection interval in milliseconds"
              " (default: 5000)\n"
           << "  --once              Submit one CPU/RAM pair and exit\n"
           << "  --help              Show this help and exit\n"
           << "  --version           Show the agent version and exit\n";
    return output.str();
}

std::optional<std::chrono::milliseconds> parse_positive_milliseconds(
    std::string_view text) noexcept {
    std::int64_t value = 0;
    const auto result = std::from_chars(
        text.data(),
        text.data() + text.size(),
        value);

    if (result.ec != std::errc{} ||
        result.ptr != text.data() + text.size() ||
        value <= 0) {
        return std::nullopt;
    }

    return std::chrono::milliseconds{value};
}

}  // namespace

int main(int argc, char* argv[]) {
    static_assert(
        std::atomic_bool::is_always_lock_free,
        "The signal stop flag must be lock-free");

    auto& logger = guardian::tools::Logger::instance();
    const bool file_logging_available = logger.configure("guardian-agent");
    logger.install_terminate_handler();
    if (!file_logging_available) {
        GUARDIAN_LOG_WARNING(
            "agent.startup",
            "File logging is unavailable; continuing with console logging");
    }

    std::string server_address{"127.0.0.1:50051"};
    guardian::agent::RunnerOptions options{};
    bool once = false;

    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};

        if (argument == "--help") {
            GUARDIAN_LOG_INFO("agent.cli", usage_text(argv[0]));
            return 0;
        }

        if (argument == "--version") {
            GUARDIAN_LOG_INFO(
                "agent.cli",
                "AI PC Guardian Agent v",
                guardian::version);
            return 0;
        }

        if (argument == "--once") {
            once = true;
            continue;
        }

        if (argument == "--server") {
            if (index + 1 >= argc) {
                GUARDIAN_LOG_ERROR(
                    "agent.cli",
                    "Missing value for --server");
                GUARDIAN_LOG_ERROR("agent.cli", usage_text(argv[0]));
                return 2;
            }

            server_address = argv[++index];
            continue;
        }

        if (argument == "--interval-ms") {
            if (index + 1 >= argc) {
                GUARDIAN_LOG_ERROR(
                    "agent.cli",
                    "Missing value for --interval-ms");
                GUARDIAN_LOG_ERROR("agent.cli", usage_text(argv[0]));
                return 2;
            }

            const auto interval =
                parse_positive_milliseconds(argv[++index]);
            if (!interval) {
                GUARDIAN_LOG_ERROR(
                    "agent.cli",
                    "--interval-ms must be a positive integer");
                return 2;
            }

            options.collection_interval = *interval;
            continue;
        }

        GUARDIAN_LOG_ERROR(
            "agent.cli",
            "Unknown argument: ",
            argument);
        GUARDIAN_LOG_ERROR("agent.cli", usage_text(argv[0]));
        return 2;
    }

    if (options.cpu_sample_interval > options.collection_interval) {
        GUARDIAN_LOG_ERROR(
            "agent.cli",
            "--interval-ms must be at least ",
            options.cpu_sample_interval.count());
        return 2;
    }

    const auto now = std::chrono::time_point_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now());
    const guardian::model::DeviceId device_id{
        "windows-agent-" +
        std::to_string(now.time_since_epoch().count())};

    const auto channel = grpc::CreateChannel(
        server_address,
        grpc::InsecureChannelCredentials());
    guardian::agent::TelemetryClient client{channel};
    guardian::agent::LatestSnapshotStore snapshot_store;
    guardian::agent::TelemetryRunner runner{
        client,
        snapshot_store,
        options,
        device_id};

    if (once) {
        const guardian::agent::CycleResult result = runner.run_once();
        if (!result.error_message.empty()) {
            GUARDIAN_LOG_ERROR(
                "agent.telemetry",
                "One-shot telemetry failed: ",
                result.error_message);
        }

        if (!result.cpu_submitted || !result.memory_submitted) {
            return 1;
        }

        GUARDIAN_LOG_INFO(
            "agent.telemetry",
            "One-shot CPU and memory telemetry submitted successfully");
        return 0;
    }

    std::signal(SIGINT, request_stop);
    std::signal(SIGTERM, request_stop);

    guardian::agent::IpcServer ipc_server{snapshot_store};
    ipc_server.start();

    GUARDIAN_LOG_INFO(
        "agent.lifecycle",
        "Telemetry collection started; interval_ms=",
        options.collection_interval.count(),
        ", server=",
        server_address,
        ". Press Ctrl+C to stop");
    runner.run(stop_requested);
    ipc_server.stop();
    GUARDIAN_LOG_INFO(
        "agent.lifecycle",
        "Telemetry agent stopped cleanly");
    return 0;
}
