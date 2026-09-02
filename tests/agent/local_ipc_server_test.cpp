#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>

#include <guardian/agent/local_ipc_server.hpp>
#include <guardian/ipc/framing.hpp>

#include <guardian/v1/local_agent.pb.h>

#include <chrono>
#include <cstddef>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>

namespace {

int failure_count = 0;

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        ++failure_count;
    }
}

std::wstring unique_pipe_name() {
    return LR"(\\.\pipe\ai_pc_guardian_test_)" +
        std::to_wstring(GetCurrentProcessId());
}

HANDLE connect_test_client(const std::wstring& pipe_name) {
    using namespace std::chrono_literals;

    for (int attempt = 0; attempt < 200; ++attempt) {
        const HANDLE client = CreateFileW(
            pipe_name.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);

        if (client != INVALID_HANDLE_VALUE) {
            return client;
        }

        std::this_thread::sleep_for(10ms);
    }

    return INVALID_HANDLE_VALUE;
}

bool write_all(HANDLE pipe, std::string_view bytes) {
    std::size_t written = 0;
    while (written < bytes.size()) {
        DWORD chunk = 0;
        if (WriteFile(
                pipe,
                bytes.data() + written,
                static_cast<DWORD>(bytes.size() - written),
                &chunk,
                nullptr) == FALSE ||
            chunk == 0) {
            return false;
        }
        written += chunk;
    }
    return true;
}

bool send_request(HANDLE pipe, std::uint32_t version = 1) {
    guardian::v1::LocalSnapshotRequest request;
    request.set_protocol_version(version);

    std::string payload;
    if (!request.SerializeToString(&payload)) {
        return false;
    }

    const auto frame = guardian::ipc::encode_frame(payload);
    return frame && write_all(pipe, *frame);
}

std::optional<guardian::v1::LocalSnapshotResponse> read_response(HANDLE pipe) {
    std::string buffer;
    while (true) {
        char bytes[256];
        DWORD bytes_read = 0;
        if (ReadFile(
                pipe,
                bytes,
                sizeof(bytes),
                &bytes_read,
                nullptr) == FALSE ||
            bytes_read == 0) {
            return std::nullopt;
        }

        buffer.append(bytes, bytes_read);
        const auto decoded = guardian::ipc::try_decode_frame(buffer);
        if (decoded.status == guardian::ipc::FrameDecodeStatus::Incomplete) {
            continue;
        }
        if (decoded.status == guardian::ipc::FrameDecodeStatus::InvalidLength) {
            return std::nullopt;
        }

        guardian::v1::LocalSnapshotResponse response;
        if (!response.ParseFromString(decoded.payload)) {
            return std::nullopt;
        }
        return response;
    }
}

}  // namespace

int main() {
    using guardian::agent::IpcServer;
    using guardian::agent::LatestSnapshot;
    using guardian::agent::LatestSnapshotStore;

    static_assert(!std::is_copy_constructible_v<IpcServer>);
    static_assert(!std::is_copy_assignable_v<IpcServer>);

    LatestSnapshotStore store;
    const std::wstring pipe_name = unique_pipe_name();

    IpcServer server{store, pipe_name};
    server.stop();
    server.start();
    server.start();

    HANDLE client = connect_test_client(pipe_name);
    expect(client != INVALID_HANDLE_VALUE, "client connects to pipe server");

    if (client != INVALID_HANDLE_VALUE) {
        expect(send_request(client), "starting request is written");
        const auto starting = read_response(client);
        expect(starting.has_value(), "starting response is received");
        if (starting) {
            expect(starting->protocol_version() == 1,
                   "response carries protocol version");
            expect(starting->agent_state() ==
                       guardian::v1::AGENT_STATE_STARTING,
                   "empty store reports STARTING");
            expect(!starting->has_snapshot(),
                   "starting response does not invent a snapshot");
        }

        store.update(LatestSnapshot{
            guardian::model::Timestamp{
                std::chrono::milliseconds{123456}},
            guardian::model::CpuMetric{42.5},
            guardian::model::MemoryMetric{16000, 6000},
            {},
            {}});

        expect(send_request(client),
               "second request uses the same connection");
        const auto running = read_response(client);
        expect(running.has_value(), "running response is received");
        if (running) {
            expect(running->agent_state() ==
                       guardian::v1::AGENT_STATE_RUNNING,
                   "complete store reports RUNNING");
            expect(running->has_snapshot(),
                   "running response contains a snapshot");
            expect(running->snapshot().collected_at_unix_ms() == 123456,
                   "timestamp is preserved");
            expect(running->snapshot().has_cpu() &&
                       running->snapshot().cpu().usage_percent() == 42.5,
                   "CPU metric is preserved");
            expect(running->snapshot().has_memory() &&
                       running->snapshot().memory().total_bytes() == 16000 &&
                       running->snapshot().memory().available_bytes() == 6000,
                   "memory metric is preserved");
        }

        store.update(LatestSnapshot{
            guardian::model::Timestamp{
                std::chrono::milliseconds{234567}},
            std::nullopt,
            guardian::model::MemoryMetric{32000, 8000},
            "CPU collection failed",
            "gRPC unavailable"});

        expect(send_request(client),
               "partial snapshot request uses the same connection");
        const auto degraded = read_response(client);
        expect(degraded.has_value(), "degraded response is received");
        if (degraded) {
            expect(degraded->agent_state() ==
                       guardian::v1::AGENT_STATE_DEGRADED,
                   "partial store reports DEGRADED");
            expect(degraded->has_snapshot() &&
                       !degraded->snapshot().has_cpu() &&
                       degraded->snapshot().has_memory(),
                   "partial response preserves the available metric");
            expect(degraded->status_message().find("CPU collection failed") !=
                       std::string::npos &&
                       degraded->status_message().find("gRPC unavailable") !=
                       std::string::npos,
                   "degraded response explains collection and submission errors");
        }

        CloseHandle(client);
    }

    client = connect_test_client(pipe_name);
    expect(client != INVALID_HANDLE_VALUE,
           "client reconnects after disconnect");
    if (client != INVALID_HANDLE_VALUE) {
        expect(send_request(client, 99),
               "unsupported-version request is written");

        char byte = 0;
        DWORD bytes_read = 0;
        const BOOL result = ReadFile(
            client,
            &byte,
            1,
            &bytes_read,
            nullptr);
        expect(result == FALSE || bytes_read == 0,
               "unsupported version closes the connection");
        CloseHandle(client);
    }

    client = connect_test_client(pipe_name);
    expect(client != INVALID_HANDLE_VALUE,
           "client connects before invalid-frame test");
    if (client != INVALID_HANDLE_VALUE) {
        const std::string invalid_frame(4, '\0');
        expect(write_all(client, invalid_frame),
               "invalid zero-length frame is written");

        char byte = 0;
        DWORD bytes_read = 0;
        const BOOL result = ReadFile(
            client,
            &byte,
            1,
            &bytes_read,
            nullptr);
        expect(result == FALSE || bytes_read == 0,
               "invalid frame closes only the client connection");
        CloseHandle(client);
    }

    client = connect_test_client(pipe_name);
    expect(client != INVALID_HANDLE_VALUE,
           "client connects before stop-during-read test");
    server.stop();
    server.stop();
    if (client != INVALID_HANDLE_VALUE) {
        CloseHandle(client);
    }

    server.start();
    server.stop();

    if (failure_count != 0) {
        std::cerr << failure_count << " test(s) failed\n";
        return 1;
    }
    return 0;
}
