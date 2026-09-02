#include <guardian/agent/windows/system_metrics_collector.hpp>

#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <string_view>

namespace {

int failure_count = 0;

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        ++failure_count;
    }
}

bool is_near(double actual, double expected) {
    return std::abs(actual - expected) < 0.000'001;
}

}  // namespace

int main() {
    using guardian::agent::windows::CpuTimes;
    using guardian::agent::windows::calculate_cpu_metric;

    const auto fifty_percent = calculate_cpu_metric(
        CpuTimes{100, 300, 200},
        CpuTimes{150, 360, 240});
    expect(fifty_percent.has_value(), "valid CPU counters produce a metric");
    expect(fifty_percent && is_near(fifty_percent->usage_percent, 50.0),
           "CPU usage is calculated from idle, kernel and user deltas");

    const auto idle_cpu = calculate_cpu_metric(
        CpuTimes{100, 300, 200},
        CpuTimes{200, 400, 200});
    expect(idle_cpu && is_near(idle_cpu->usage_percent, 0.0),
           "fully idle interval produces zero percent");

    const auto busy_cpu = calculate_cpu_metric(
        CpuTimes{100, 300, 200},
        CpuTimes{100, 350, 250});
    expect(busy_cpu && is_near(busy_cpu->usage_percent, 100.0),
           "interval without idle time produces one hundred percent");

    expect(!calculate_cpu_metric(
                CpuTimes{100, 300, 200},
                CpuTimes{100, 300, 200}),
           "zero total delta is rejected");
    expect(!calculate_cpu_metric(
                CpuTimes{100, 300, 200},
                CpuTimes{99, 310, 210}),
           "decreasing idle counter is rejected");
    expect(!calculate_cpu_metric(
                CpuTimes{100, 300, 200},
                CpuTimes{101, 299, 210}),
           "decreasing kernel counter is rejected");
    expect(!calculate_cpu_metric(
                CpuTimes{100, 300, 200},
                CpuTimes{101, 310, 199}),
           "decreasing user counter is rejected");
    expect(!calculate_cpu_metric(
                CpuTimes{100, 300, 200},
                CpuTimes{121, 310, 210}),
           "idle delta greater than total delta is rejected");
    expect(!calculate_cpu_metric(
                CpuTimes{0, 0, 0},
                CpuTimes{0, std::numeric_limits<std::uint64_t>::max(), 1}),
           "overflowing total delta is rejected");

    expect(!guardian::agent::windows::collect_cpu_metric(
                std::chrono::milliseconds::zero()),
           "zero sample interval is rejected");
    expect(!guardian::agent::windows::collect_cpu_metric(
                std::chrono::milliseconds{-1}),
           "negative sample interval is rejected");

    const auto real_cpu = guardian::agent::windows::collect_cpu_metric(
        std::chrono::milliseconds{50});
    expect(real_cpu.has_value(), "GetSystemTimes produces a CPU metric");
    expect(real_cpu && real_cpu->usage_percent >= 0.0 &&
               real_cpu->usage_percent <= 100.0,
           "real CPU metric stays in the valid range");

    const auto real_memory =
        guardian::agent::windows::collect_memory_metric();
    expect(real_memory.has_value(),
           "GlobalMemoryStatusEx produces a memory metric");
    expect(real_memory && real_memory->total_bytes > 0,
           "real total physical memory is positive");
    expect(real_memory &&
               real_memory->available_bytes <= real_memory->total_bytes,
           "available physical memory does not exceed total memory");

    if (failure_count != 0) {
        std::cerr << failure_count << " test(s) failed\n";
        return 1;
    }

    return 0;
}
