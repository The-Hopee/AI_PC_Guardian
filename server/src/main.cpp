#include <guardian/server/telemetry_service.hpp>
#include <guardian/tools/logger.hpp>
#include <guardian/version.hpp>

#include <grpcpp/grpcpp.h>

#include <chrono>
#include <csignal>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>

namespace {

volatile std::sig_atomic_t stop_requested = 0;

void request_stop(int) {
    stop_requested = 1;
}

std::string usage_text(std::string_view program_name) {
    std::ostringstream output;
    output << "Usage: " << program_name << " [--address HOST:PORT]\n"
           << "\n"
           << "Options:\n"
           << "  --address HOST:PORT  Listening address (default: 127.0.0.1:50051)\n"
           << "  --help               Show this help and exit\n"
           << "  --version            Show the server version and exit\n";
    return output.str();
}

}  // namespace

int main(int argc, char* argv[]) {
    auto& logger = guardian::tools::Logger::instance();
    const bool file_logging_available = logger.configure("guardian-server");
    logger.install_terminate_handler();
    if (!file_logging_available) {
        GUARDIAN_LOG_WARNING(
            "server.startup",
            "File logging is unavailable; continuing with console logging");
    }

    std::string server_address{"127.0.0.1:50051"};

    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};

        if (argument == "--help") {
            GUARDIAN_LOG_INFO("server.cli", usage_text(argv[0]));
            return 0;
        }

        if (argument == "--version") {
            GUARDIAN_LOG_INFO(
                "server.cli",
                "AI PC Guardian Server v",
                guardian::version);
            return 0;
        }

        if (argument == "--address") {
            if (index + 1 >= argc) {
                GUARDIAN_LOG_ERROR(
                    "server.cli",
                    "Missing value for --address");
                GUARDIAN_LOG_ERROR("server.cli", usage_text(argv[0]));
                return 2;
            }

            server_address = argv[++index];
            continue;
        }

        GUARDIAN_LOG_ERROR(
            "server.cli",
            "Unknown argument: ",
            argument);
        GUARDIAN_LOG_ERROR("server.cli", usage_text(argv[0]));
        return 2;
    }

    guardian::server::TelemetryServiceImpl service;

    ::grpc::ServerBuilder builder;
    // AddListeningPort writes the actually bound port here during BuildAndStart().
    // A value of zero afterwards means that gRPC failed to bind the address.
    int bound_port = 0;
    builder.AddListeningPort(
        server_address,
        ::grpc::InsecureServerCredentials(),
        &bound_port);
    builder.RegisterService(&service);

    std::unique_ptr<::grpc::Server> server = builder.BuildAndStart();

    if (!server || bound_port == 0) {
        GUARDIAN_LOG_CRITICAL(
            "server.startup",
            "Failed to start gRPC server; address=",
            server_address,
            ", bound_port=",
            bound_port);
        return 1;
    }

    std::signal(SIGINT, request_stop);
    std::signal(SIGTERM, request_stop);

    GUARDIAN_LOG_INFO(
        "server.lifecycle",
        "gRPC server is listening; address=",
        server_address,
        ", bound_port=",
        bound_port);

    while (stop_requested == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
    }

    GUARDIAN_LOG_INFO(
        "server.lifecycle",
        "Shutdown signal received; stopping gRPC server");
    server->Shutdown();
    server->Wait();
    GUARDIAN_LOG_INFO("server.lifecycle", "gRPC server stopped cleanly");

    return 0;
}
