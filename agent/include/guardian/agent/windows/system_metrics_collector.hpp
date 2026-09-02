#pragma once

#include <guardian/model/metrics.hpp>

#include <chrono>
#include <cstdint>
#include <optional>

namespace guardian::agent::windows {

struct CpuTimes {
    std::uint64_t idle;
    std::uint64_t kernel;
    std::uint64_t user;
};

[[nodiscard]]
std::optional<guardian::model::CpuMetric> calculate_cpu_metric(
    const CpuTimes& previous,
    const CpuTimes& current) noexcept;

[[nodiscard]]
std::optional<guardian::model::CpuMetric> collect_cpu_metric(
    std::chrono::milliseconds sample_interval);

[[nodiscard]]
std::optional<guardian::model::MemoryMetric> collect_memory_metric() noexcept;

}  // namespace guardian::agent::windows
