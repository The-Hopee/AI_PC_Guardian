#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace guardian::model {

struct EventId {
    std::string value;
};

struct DeviceId {
    std::string value;
};

struct ProcessId {
    std::uint32_t value;
};

using Timestamp = std::chrono::time_point<
    std::chrono::system_clock,
    std::chrono::milliseconds>;

}  // namespace guardian::model
