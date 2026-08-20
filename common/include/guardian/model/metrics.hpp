#pragma once

#include <cstdint>

namespace guardian::model {

struct CpuMetric {
    double usage_percent;
};

struct MemoryMetric {
    std::uint64_t total_bytes;
    std::uint64_t available_bytes;

    [[nodiscard]]
    std::uint64_t used_bytes() const noexcept;
};

}  // namespace guardian::model
