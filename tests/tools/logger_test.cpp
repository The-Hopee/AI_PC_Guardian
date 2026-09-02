#include <guardian/tools/logger.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
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

}  // namespace

int main() {
    const auto unique_suffix = std::chrono::steady_clock::now()
        .time_since_epoch()
        .count();
    const std::filesystem::path test_directory =
        std::filesystem::temp_directory_path() /
        ("guardian-logger-test-" + std::to_string(unique_suffix));

    auto& logger = guardian::tools::Logger::instance();
    expect(
        logger.configure("logger-test", test_directory),
        "logger creates its directory and opens a file");

    GUARDIAN_LOG_DEBUG("test.logger", "debug-marker=1");
    GUARDIAN_LOG_INFO("test.logger", "info-marker=2");
    GUARDIAN_LOG_WARNING("test.logger", "warning-marker=3");
    GUARDIAN_LOG_ERROR("test.logger", "error-marker=4");
    GUARDIAN_LOG_CRITICAL("test.logger", "critical-marker=5");

    std::vector<std::thread> writers;
    for (int writer = 0; writer < 4; ++writer) {
        writers.emplace_back([writer] {
            for (int entry = 0; entry < 25; ++entry) {
                GUARDIAN_LOG_INFO(
                    "test.concurrent",
                    "writer=",
                    writer,
                    ", entry=",
                    entry);
            }
        });
    }

    for (auto& writer : writers) {
        writer.join();
    }

    logger.flush();
    const auto log_path = logger.log_file_path();
    expect(
        log_path == test_directory / "logger-test.log",
        "configured application name determines log filename");

    std::ifstream input{log_path};
    const std::string contents{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}};

    expect(contents.find("[DEBUG]") != std::string::npos,
           "debug level is written");
    expect(contents.find("[INFO]") != std::string::npos,
           "info level is written");
    expect(contents.find("[WARN]") != std::string::npos,
           "warning level is written");
    expect(contents.find("[ERROR]") != std::string::npos,
           "error level is written");
    expect(contents.find("[CRITICAL]") != std::string::npos,
           "critical level is written");
    expect(contents.find("[test.logger]") != std::string::npos,
           "component name is written");
    expect(contents.find("logger_test.cpp:") != std::string::npos,
           "source filename and line are written");
    expect(contents.find("writer=0, entry=24") != std::string::npos &&
               contents.find("writer=3, entry=24") != std::string::npos,
           "concurrent writers preserve complete entries");

    std::error_code cleanup_error;
    std::filesystem::remove_all(test_directory, cleanup_error);

    if (failure_count != 0) {
        std::cerr << failure_count << " test(s) failed\n";
        return 1;
    }

    return 0;
}
