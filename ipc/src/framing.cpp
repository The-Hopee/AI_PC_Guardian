#include <guardian/ipc/framing.hpp>

#include <array>

namespace guardian::ipc {

namespace {

constexpr std::size_t frame_header_size = 4;

}  // namespace

[[nodiscard]]
std::optional<std::string> encode_frame(std::string_view payload) {
    if (payload.empty() || payload.size() > max_frame_payload_size) {
        return std::nullopt;
    }

    const auto size = static_cast<std::uint32_t>(payload.size());
    std::array<char, frame_header_size> frame{};

    frame[0] = static_cast<char>(size & 0xFF);
    frame[1] = static_cast<char>((size >> 8) & 0xFF);
    frame[2] = static_cast<char>((size >> 16) & 0xFF);
    frame[3] = static_cast<char>((size >> 24) & 0xFF);

    std::string result;

    result.reserve(frame.size() + payload.size());

    result.append(frame.data(), frame.size());
    result.append(payload.data(), payload.size());

    return result;
}

[[nodiscard]]
FrameDecodeResult try_decode_frame(std::string_view buffer) {
    FrameDecodeResult result{};

    if (buffer.size() < frame_header_size) {
        return result;
    }

    const auto byte = [](char value) {
        return static_cast<std::uint32_t>(
            static_cast<unsigned char>(value));
    };

    const std::uint32_t payload_size =
        byte(buffer[0]) |
        (byte(buffer[1]) << 8) |
        (byte(buffer[2]) << 16) |
        (byte(buffer[3]) << 24);

    if (payload_size == 0 ||
        payload_size > max_frame_payload_size) {
        result.status = FrameDecodeStatus::InvalidLength;
        return result;
    }

    const std::size_t frame_size = frame_header_size + payload_size;

    if (buffer.size() < frame_size) {
        return result;
    }

    result.status = FrameDecodeStatus::Complete;
    result.consumed_bytes = frame_size;
    result.payload = std::string{
        buffer.substr(frame_header_size, payload_size)};

    return result;
}

}  // namespace guardian::ipc
