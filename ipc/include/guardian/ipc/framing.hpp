#pragma once

#include <cstdint>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace guardian::ipc {

inline constexpr std::uint32_t max_frame_payload_size =
    1024 * 1024;

enum class FrameDecodeStatus {
    Incomplete,
    Complete,
    InvalidLength
};

struct FrameDecodeResult {
    FrameDecodeStatus status{FrameDecodeStatus::Incomplete};
    std::size_t consumed_bytes{0};
    std::string payload;
};

[[nodiscard]]
std::optional<std::string> encode_frame(std::string_view payload);

[[nodiscard]]
FrameDecodeResult try_decode_frame(std::string_view buffer);

}  // namespace guardian::ipc
