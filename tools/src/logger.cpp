#include <guardian/tools/logger.hpp>

#include <chrono>
#include <cstdlib>
#include <exception>
#include <iomanip>
#include <iostream>
#include <system_error>
#include <thread>

namespace {

std::string_view level_name(guardian::tools::LogLevel level) noexcept {
    using guardian::tools::LogLevel;

    switch (level) {
    case LogLevel::Debug:
        return "DEBUG";
    case LogLevel::Info:
        return "INFO";
    case LogLevel::Warning:
        return "WARN";
    case LogLevel::Error:
        return "ERROR";
    case LogLevel::Critical:
        return "CRITICAL";
    }

    return "UNKNOWN";
}

std::string timestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto milliseconds = std::chrono::duration_cast<
        std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm local_time{};

#ifdef _WIN32
    localtime_s(&local_time, &time);
#else
    localtime_r(&time, &local_time);
#endif

    std::ostringstream output;
    output << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S")
           << '.' << std::setfill('0') << std::setw(3)
           << milliseconds.count();
    return output.str();
}

std::string_view source_filename(const char* source_file) noexcept {
    if (source_file == nullptr) {
        return {};
    }

    std::string_view path{source_file};
    const auto separator = path.find_last_of("/\\");
    if (separator == std::string_view::npos) {
        return path;
    }

    return path.substr(separator + 1);
}

std::string sanitize_application_name(std::string name) {
    if (name.empty()) {
        return "guardian";
    }

    for (char& character : name) {
        const bool valid =
            (character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') ||
            character == '-' || character == '_';
        if (!valid) {
            character = '_';
        }
    }

    return name;
}

std::filesystem::path default_log_directory() {
#ifdef _WIN32
    char* local_app_data = nullptr;
    std::size_t local_app_data_size = 0;
    const errno_t environment_result = _dupenv_s(
        &local_app_data,
        &local_app_data_size,
        "LOCALAPPDATA");

    if (environment_result == 0 &&
        local_app_data != nullptr &&
        local_app_data_size > 1) {
        const std::filesystem::path result =
            std::filesystem::path{local_app_data} /
            "AI_PC_Guardian" / "logs";
        std::free(local_app_data);
        return result;
    }
    std::free(local_app_data);
#else
    if (const char* state_home = std::getenv("XDG_STATE_HOME")) {
        if (*state_home != '\0') {
            return std::filesystem::path{state_home} /
                "ai-pc-guardian" / "logs";
        }
    }

    if (const char* user_home = std::getenv("HOME")) {
        if (*user_home != '\0') {
            return std::filesystem::path{user_home} /
                ".local" / "state" / "ai-pc-guardian" / "logs";
        }
    }
#endif

    return std::filesystem::current_path() / "logs";
}

}  // namespace

namespace guardian::tools {

Logger& Logger::instance() noexcept {
    static Logger logger;
    return logger;
}

Logger::Logger() noexcept = default;

bool Logger::configure(
    std::string application_name,
    std::filesystem::path log_directory) noexcept {
    try {
        std::lock_guard lock{m_mutex};

        if (log_directory.empty()) {
            log_directory = default_log_directory();
        }

        std::error_code error;
        std::filesystem::create_directories(log_directory, error);
        if (error) {
            return false;
        }

        m_log_file.close();
        m_log_file.clear();
        m_log_file_path = log_directory /
            (sanitize_application_name(std::move(application_name)) + ".log");
        m_log_file.open(m_log_file_path, std::ios::app);

        if (!m_log_file.is_open()) {
            return false;
        }

        m_log_file << '\n' << timestamp()
                   << " [INFO] [logger] Logging session started\n";
        m_log_file.flush();
        return true;
    } catch (...) {
        return false;
    }
}

void Logger::install_terminate_handler() noexcept {
    std::set_terminate([] {
        auto& logger = Logger::instance();

        std::string reason{"std::terminate called"};
        if (const auto exception = std::current_exception()) {
            try {
                std::rethrow_exception(exception);
            } catch (const std::exception& error) {
                reason.append(": ");
                reason.append(error.what());
            } catch (...) {
                reason.append(": unknown exception");
            }
        }

        logger.log(
            LogLevel::Critical,
            "runtime",
            __FILE__,
            __LINE__,
            reason);
        logger.flush();
        std::abort();
    });
}

void Logger::write(
    LogLevel level,
    std::string_view component,
    std::string_view message,
    const char* source_file,
    int source_line) noexcept {
    try {
        std::lock_guard lock{m_mutex};

        std::ostringstream prefix;
        prefix << timestamp()
               << " [" << level_name(level) << ']'
               << " [thread " << std::this_thread::get_id() << ']'
               << " [" << component << "] ";

        std::ostringstream suffix;
        suffix << " (" << source_filename(source_file)
               << ':' << source_line << ')';

        std::size_t offset = 0;
        do {
            const auto newline = message.find('\n', offset);
            const auto line = message.substr(
                offset,
                newline == std::string_view::npos
                    ? message.size() - offset
                    : newline - offset);

            std::ostream& console =
                level == LogLevel::Error || level == LogLevel::Critical
                    ? std::cerr
                    : std::clog;
            console << prefix.str() << line << suffix.str() << '\n';
            console.flush();

            if (m_log_file.is_open()) {
                m_log_file << prefix.str() << line << suffix.str() << '\n';
            }

            if (newline == std::string_view::npos) {
                break;
            }
            offset = newline + 1;
        } while (offset < message.size());

        if (m_log_file.is_open()) {
            m_log_file.flush();
        }
    } catch (...) {
        // Logging failures are intentionally isolated from application logic.
    }
}

void Logger::flush() noexcept {
    try {
        std::lock_guard lock{m_mutex};
        std::clog.flush();
        std::cerr.flush();
        if (m_log_file.is_open()) {
            m_log_file.flush();
        }
    } catch (...) {
    }
}

std::filesystem::path Logger::log_file_path() const {
    std::lock_guard lock{m_mutex};
    return m_log_file_path;
}

}  // namespace guardian::tools
