#include <guardian/ipc/framing.hpp>

#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>

namespace {

int failure_count = 0;

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        ++failure_count;
    }
}

std::string length_header(std::uint32_t size) {
    std::string header(4, '\0');
    header[0] = static_cast<char>(size & 0xFF);
    header[1] = static_cast<char>((size >> 8) & 0xFF);
    header[2] = static_cast<char>((size >> 16) & 0xFF);
    header[3] = static_cast<char>((size >> 24) & 0xFF);
    return header;
}

}  // namespace

int main() {
    using guardian::ipc::FrameDecodeStatus;

    expect(!guardian::ipc::encode_frame("").has_value(),
           "empty payload is rejected");

    const std::string oversized(
        guardian::ipc::max_frame_payload_size + 1,
        'x');
    expect(!guardian::ipc::encode_frame(oversized).has_value(),
           "payload above the maximum is rejected");

    const std::string maximum(
        guardian::ipc::max_frame_payload_size,
        'm');
    const auto maximum_frame = guardian::ipc::encode_frame(maximum);
    expect(maximum_frame.has_value() &&
               maximum_frame->size() == maximum.size() + 4,
           "payload exactly at the maximum is accepted");

    const std::string binary_payload{"A\0B", 3};
    const auto binary_frame = guardian::ipc::encode_frame(binary_payload);
    expect(binary_frame.has_value(),
           "binary payload encodes");
    if (binary_frame) {
        expect(binary_frame->size() == 7,
               "encoded frame contains header and payload");
        expect(static_cast<unsigned char>((*binary_frame)[0]) == 3 &&
                   (*binary_frame)[1] == '\0' &&
                   (*binary_frame)[2] == '\0' &&
                   (*binary_frame)[3] == '\0',
               "encoded frame uses little-endian length");
        expect(binary_frame->substr(4) == binary_payload,
               "encoded frame preserves embedded null bytes");
    }

    const auto short_header = guardian::ipc::try_decode_frame("\x03\x00");
    expect(short_header.status == FrameDecodeStatus::Incomplete &&
               short_header.consumed_bytes == 0 &&
               short_header.payload.empty(),
           "partial header is incomplete");

    const auto zero_length = guardian::ipc::try_decode_frame(
        std::string(4, '\0'));
    expect(zero_length.status == FrameDecodeStatus::InvalidLength,
           "zero payload length is invalid");

    const auto oversized_length = guardian::ipc::try_decode_frame(
        length_header(guardian::ipc::max_frame_payload_size + 1));
    expect(oversized_length.status == FrameDecodeStatus::InvalidLength,
           "payload length above the maximum is invalid");

    std::string incomplete_payload = length_header(5);
    incomplete_payload.append("abc", 3);
    const auto incomplete =
        guardian::ipc::try_decode_frame(incomplete_payload);
    expect(incomplete.status == FrameDecodeStatus::Incomplete &&
               incomplete.consumed_bytes == 0,
           "partial payload is incomplete");

    if (binary_frame) {
        const auto decoded = guardian::ipc::try_decode_frame(*binary_frame);
        expect(decoded.status == FrameDecodeStatus::Complete,
               "complete binary frame decodes");
        expect(decoded.consumed_bytes == binary_frame->size(),
               "complete frame reports consumed byte count");
        expect(decoded.payload == binary_payload,
               "decoded payload preserves embedded null bytes");
    }

    const std::string long_payload(300, 'p');
    const auto long_frame = guardian::ipc::encode_frame(long_payload);
    expect(long_frame.has_value(),
           "multi-byte payload length encodes");
    if (long_frame) {
        const auto decoded = guardian::ipc::try_decode_frame(*long_frame);
        expect(decoded.status == FrameDecodeStatus::Complete &&
                   decoded.consumed_bytes == long_frame->size() &&
                   decoded.payload == long_payload,
               "multi-byte little-endian length decodes");
    }

    const auto first_frame = guardian::ipc::encode_frame("first");
    const auto second_frame = guardian::ipc::encode_frame("second");
    if (first_frame && second_frame) {
        const std::string combined = *first_frame + *second_frame;
        const auto first = guardian::ipc::try_decode_frame(combined);
        expect(first.status == FrameDecodeStatus::Complete &&
                   first.payload == "first" &&
                   first.consumed_bytes == first_frame->size(),
               "decoder consumes only the first of multiple frames");
        const auto second = guardian::ipc::try_decode_frame(
            std::string_view{combined}.substr(first.consumed_bytes));
        expect(second.status == FrameDecodeStatus::Complete &&
                   second.payload == "second" &&
                   second.consumed_bytes == second_frame->size(),
               "remaining frame decodes after consuming the first");
    }

    if (failure_count != 0) {
        std::cerr << failure_count << " test(s) failed\n";
        return 1;
    }

    return 0;
}
