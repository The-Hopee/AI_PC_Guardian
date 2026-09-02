#pragma once

#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace guardian::tools {

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error,
    Critical
};

class Logger final {
public:
    static Logger& instance() noexcept;

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    [[nodiscard]] bool configure(
        std::string application_name,
        std::filesystem::path log_directory = {}) noexcept;

    void install_terminate_handler() noexcept;

    template <typename... Arguments>
    void log(
        LogLevel level,
        std::string_view component,
        const char* source_file,
        int source_line,
        Arguments&&... arguments) noexcept {
        try {
            std::ostringstream message;
            (message << ... << std::forward<Arguments>(arguments));
            write(
                level,
                component,
                message.str(),
                source_file,
                source_line);
        } catch (...) {
            // Logging must never terminate the application it observes.
        }
    }

    void flush() noexcept;

    [[nodiscard]] std::filesystem::path log_file_path() const;

private:
    Logger() noexcept;
    ~Logger() = default;

    void write(
        LogLevel level,
        std::string_view component,
        std::string_view message,
        const char* source_file,
        int source_line) noexcept;

    mutable std::mutex m_mutex;
    std::ofstream m_log_file;
    std::filesystem::path m_log_file_path;
};

}  // namespace guardian::tools

#define GUARDIAN_LOG_DEBUG(component, ...)                                \
    ::guardian::tools::Logger::instance().log(                            \
        ::guardian::tools::LogLevel::Debug,                               \
        component, __FILE__, __LINE__, __VA_ARGS__)

#define GUARDIAN_LOG_INFO(component, ...)                                 \
    ::guardian::tools::Logger::instance().log(                            \
        ::guardian::tools::LogLevel::Info,                                \
        component, __FILE__, __LINE__, __VA_ARGS__)

#define GUARDIAN_LOG_WARNING(component, ...)                              \
    ::guardian::tools::Logger::instance().log(                            \
        ::guardian::tools::LogLevel::Warning,                             \
        component, __FILE__, __LINE__, __VA_ARGS__)

#define GUARDIAN_LOG_ERROR(component, ...)                                \
    ::guardian::tools::Logger::instance().log(                            \
        ::guardian::tools::LogLevel::Error,                               \
        component, __FILE__, __LINE__, __VA_ARGS__)

#define GUARDIAN_LOG_CRITICAL(component, ...)                             \
    ::guardian::tools::Logger::instance().log(                            \
        ::guardian::tools::LogLevel::Critical,                            \
        component, __FILE__, __LINE__, __VA_ARGS__)
