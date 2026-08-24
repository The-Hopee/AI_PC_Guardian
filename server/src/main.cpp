#include <guardian/server/telemetry_service.hpp>
#include <guardian/version.hpp>

#include <grpcpp/grpcpp.h>

#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <thread>

namespace {

volatile std::sig_atomic_t stop_requested = 0;

void request_stop(int) {
    stop_requested = 1;
}

void print_usage(std::ostream& output, std::string_view program_name) {
    output << "Usage: " << program_name << " [--address HOST:PORT]\n"
           << "\n"
           << "Options:\n"
           << "  --address HOST:PORT  Listening address (default: 127.0.0.1:50051)\n"
           << "  --help               Show this help and exit\n"
           << "  --version            Show the server version and exit\n";
}

}  // namespace

int main(int argc, char* argv[]) {
    std::string server_address{"127.0.0.1:50051"};

    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};

        if (argument == "--help") {
            print_usage(std::cout, argv[0]);
            return 0;
        }

        if (argument == "--version") {
            std::cout << "AI PC Guardian Server v" << guardian::version << '\n';
            return 0;
        }

        if (argument == "--address") {
            if (index + 1 >= argc) {
                std::cerr << "Missing value for --address\n";
                print_usage(std::cerr, argv[0]);
                return 2;
            }

            server_address = argv[++index];
            continue;
        }

        std::cerr << "Unknown argument: " << argument << '\n';
        print_usage(std::cerr, argv[0]);
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
        std::cerr << "Failed to start server on " << server_address << '\n';
        return 1;
    }

    std::signal(SIGINT, request_stop);
    std::signal(SIGTERM, request_stop);

    std::cout << "Server listening on " << server_address << std::endl;

    while (stop_requested == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
    }

    std::cout << "Stopping server\n";
    server->Shutdown();
    server->Wait();

    return 0;
}
