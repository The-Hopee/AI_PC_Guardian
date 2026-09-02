#include <guardian/agent/windows/system_metrics_collector.hpp>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <cmath>
#include <limits>
#include <thread>

namespace guardian::agent::windows {

namespace {

std::uint64_t file_time_to_uint64(const FILETIME& time) noexcept {
    return (static_cast<std::uint64_t>(time.dwHighDateTime) << 32U) |
           static_cast<std::uint64_t>(time.dwLowDateTime);
}

std::optional<CpuTimes> read_cpu_times() noexcept {
    FILETIME idle{};
    FILETIME kernel{};
    FILETIME user{};

    if (GetSystemTimes(&idle, &kernel, &user) == 0) {
        return std::nullopt;
    }

    return CpuTimes{
        file_time_to_uint64(idle),
        file_time_to_uint64(kernel),
        file_time_to_uint64(user),
    };
}

}  // namespace

std::optional<guardian::model::CpuMetric> calculate_cpu_metric(
    const CpuTimes& previous,
    const CpuTimes& current) noexcept {
    if (current.idle < previous.idle ||
        current.kernel < previous.kernel ||
        current.user < previous.user) {
        return std::nullopt;
    }

    const std::uint64_t idle_delta = current.idle - previous.idle;
    const std::uint64_t kernel_delta = current.kernel - previous.kernel;
    const std::uint64_t user_delta = current.user - previous.user;

    if (kernel_delta >
        std::numeric_limits<std::uint64_t>::max() - user_delta) {
        return std::nullopt;
    }

    const std::uint64_t total_delta = kernel_delta + user_delta;
    if (total_delta == 0 || idle_delta > total_delta) {
        return std::nullopt;
    }

    const std::uint64_t busy_delta = total_delta - idle_delta;
    const double usage_percent =
        static_cast<double>(busy_delta) /
        static_cast<double>(total_delta) * 100.0;

    if (!std::isfinite(usage_percent) ||
        usage_percent < 0.0 || usage_percent > 100.0) {
        return std::nullopt;
    }

    return guardian::model::CpuMetric{usage_percent};
}

std::optional<guardian::model::CpuMetric> collect_cpu_metric(
    std::chrono::milliseconds sample_interval) {
    if (sample_interval <= std::chrono::milliseconds::zero()) {
        return std::nullopt;
    }

    const auto previous = read_cpu_times();
    if (!previous) {
        return std::nullopt;
    }

    std::this_thread::sleep_for(sample_interval);

    const auto current = read_cpu_times();
    if (!current) {
        return std::nullopt;
    }

    return calculate_cpu_metric(*previous, *current);
}

std::optional<guardian::model::MemoryMetric> collect_memory_metric() noexcept {
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);

    if (GlobalMemoryStatusEx(&status) == 0) {
        return std::nullopt;
    }

    const guardian::model::MemoryMetric metric{
        static_cast<std::uint64_t>(status.ullTotalPhys),
        static_cast<std::uint64_t>(status.ullAvailPhys),
    };

    if (metric.total_bytes == 0 ||
        metric.available_bytes > metric.total_bytes) {
        return std::nullopt;
    }

    return metric;
}

}  // namespace guardian::agent::windows
