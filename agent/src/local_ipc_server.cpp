#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>
#include <sddl.h>

#include <guardian/agent/local_ipc_server.hpp>
#include <guardian/ipc/framing.hpp>
#include <guardian/tools/logger.hpp>

#include <guardian/v1/local_agent.pb.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace {

constexpr std::uint32_t protocol_version = 1;
constexpr DWORD pipe_buffer_size = 64 * 1024;

class LocalSecurityDescriptor {
public:
    LocalSecurityDescriptor() {
        constexpr wchar_t descriptor_text[] =
            L"D:P(A;;GA;;;SY)(A;;GA;;;OW)";

        if (ConvertStringSecurityDescriptorToSecurityDescriptorW(
                descriptor_text,
                SDDL_REVISION_1,
                &descriptor_,
                nullptr) == FALSE) {
            error_ = GetLastError();
        }
    }

    ~LocalSecurityDescriptor() {
        if (descriptor_ != nullptr) {
            LocalFree(descriptor_);
        }
    }

    LocalSecurityDescriptor(const LocalSecurityDescriptor&) = delete;
    LocalSecurityDescriptor& operator=(const LocalSecurityDescriptor&) = delete;

    [[nodiscard]] bool valid() const noexcept {
        return descriptor_ != nullptr;
    }

    [[nodiscard]] DWORD error() const noexcept {
        return error_;
    }

    [[nodiscard]] SECURITY_ATTRIBUTES attributes() const noexcept {
        SECURITY_ATTRIBUTES result{};
        result.nLength = sizeof(result);
        result.lpSecurityDescriptor = descriptor_;
        result.bInheritHandle = FALSE;
        return result;
    }

private:
    PSECURITY_DESCRIPTOR descriptor_{nullptr};
    DWORD error_{ERROR_SUCCESS};
};

void append_status(std::string& destination, std::string_view message) {
    if (!destination.empty()) {
        destination += "; ";
    }
    destination.append(message.data(), message.size());
}

guardian::v1::LocalSnapshotResponse make_response(
    const guardian::agent::LatestSnapshotStore& store) {
    guardian::v1::LocalSnapshotResponse response;
    response.set_protocol_version(protocol_version);

    const auto snapshot = store.get();
    if (!snapshot) {
        response.set_agent_state(guardian::v1::AGENT_STATE_STARTING);
        response.set_status_message("Waiting for the first metric snapshot");
        return response;
    }

    const bool complete = snapshot->cpu && snapshot->memory;
    const bool has_errors =
        !snapshot->collection_error.empty() ||
        !snapshot->submission_error.empty();

    if (complete && !has_errors) {
        response.set_agent_state(guardian::v1::AGENT_STATE_RUNNING);
        response.set_status_message("System is under observation");
    } else {
        response.set_agent_state(guardian::v1::AGENT_STATE_DEGRADED);

        std::string status_message;
        if (!snapshot->cpu) {
            append_status(status_message, "CPU metric is unavailable");
        }
        if (!snapshot->memory) {
            append_status(status_message, "Memory metric is unavailable");
        }
        if (!snapshot->collection_error.empty()) {
            append_status(
                status_message,
                "Collection error: " + snapshot->collection_error);
        }
        if (!snapshot->submission_error.empty()) {
            append_status(
                status_message,
                "Remote submission error: " + snapshot->submission_error);
        }
        response.set_status_message(status_message);
    }

    auto* proto_snapshot = response.mutable_snapshot();
    proto_snapshot->set_collected_at_unix_ms(
        snapshot->collected_at.time_since_epoch().count());

    if (snapshot->cpu) {
        proto_snapshot->mutable_cpu()->set_usage_percent(
            snapshot->cpu->usage_percent);
    }

    if (snapshot->memory) {
        auto* proto_memory = proto_snapshot->mutable_memory();
        proto_memory->set_total_bytes(snapshot->memory->total_bytes);
        proto_memory->set_available_bytes(snapshot->memory->available_bytes);
    }

    return response;
}

std::optional<std::string> make_response_frame(
    const guardian::agent::LatestSnapshotStore& store) {
    const auto response = make_response(store);
    std::string payload;

    if (!response.SerializeToString(&payload)) {
        GUARDIAN_LOG_ERROR(
            "agent.ipc",
            "Failed to serialize LocalSnapshotResponse");
        return std::nullopt;
    }

    auto frame = guardian::ipc::encode_frame(payload);
    if (!frame) {
        GUARDIAN_LOG_ERROR(
            "agent.ipc",
            "Failed to encode LocalSnapshotResponse frame; payload_size=",
            payload.size());
    }
    return frame;
}

bool write_all(
    HANDLE pipe,
    std::string_view frame,
    const std::atomic<bool>& stop_requested) {
    std::size_t total_written = 0;

    while (total_written < frame.size() && !stop_requested.load()) {
        DWORD bytes_written = 0;
        const auto remaining = static_cast<DWORD>(
            frame.size() - total_written);

        const BOOL result = WriteFile(
            pipe,
            frame.data() + total_written,
            remaining,
            &bytes_written,
            nullptr);

        if (result == FALSE) {
            const DWORD error = GetLastError();
            if (error != ERROR_OPERATION_ABORTED ||
                !stop_requested.load()) {
                GUARDIAN_LOG_WARNING(
                    "agent.ipc",
                    "Failed to write IPC response; win32_error=",
                    error,
                    ", bytes_sent=",
                    total_written);
            }
            return false;
        }

        if (bytes_written == 0) {
            GUARDIAN_LOG_WARNING(
                "agent.ipc",
                "IPC response write returned zero bytes");
            return false;
        }

        total_written += bytes_written;
    }

    if (total_written != frame.size()) {
        return false;
    }

    GUARDIAN_LOG_DEBUG(
        "agent.ipc",
        "LocalSnapshotResponse sent; frame_size=",
        total_written);
    return true;
}

bool process_request(
    std::string_view payload,
    HANDLE pipe,
    const guardian::agent::LatestSnapshotStore& store,
    const std::atomic<bool>& stop_requested) {
    guardian::v1::LocalSnapshotRequest request;
    if (!request.ParseFromArray(
            payload.data(),
            static_cast<int>(payload.size()))) {
        GUARDIAN_LOG_WARNING(
            "agent.ipc",
            "Rejected IPC request: failed to parse "
            "LocalSnapshotRequest protobuf payload");
        return false;
    }

    if (request.protocol_version() != protocol_version) {
        GUARDIAN_LOG_WARNING(
            "agent.ipc",
            "Rejected IPC request: unsupported protocol version; received=",
            request.protocol_version(),
            ", expected=",
            protocol_version);
        return false;
    }

    const auto response_frame = make_response_frame(store);
    return response_frame &&
        write_all(pipe, *response_frame, stop_requested);
}

void wake_pipe_listener(const std::wstring& pipe_name) noexcept {
    const HANDLE client = CreateFileW(
        pipe_name.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (client != INVALID_HANDLE_VALUE) {
        CloseHandle(client);
    }
}

}  // namespace

namespace guardian::agent {

IpcServer::IpcServer(
    const LatestSnapshotStore& store,
    std::wstring pipe_name)
    : m_store(store),
      m_pipe_name(std::move(pipe_name)) {
}

IpcServer::~IpcServer() {
    stop();
}

void IpcServer::start() {
    if (m_worker.joinable()) {
        GUARDIAN_LOG_WARNING(
            "agent.ipc",
            "Ignored duplicate IPC server start request");
        return;
    }

    m_stop_requested.store(false);
    m_worker = std::thread([this] {
        run();
    });

    GUARDIAN_LOG_INFO("agent.ipc", "IPC server worker started");
}

void IpcServer::stop() {
    m_stop_requested.store(true);

    if (!m_worker.joinable()) {
        return;
    }

    wake_pipe_listener(m_pipe_name);
    (void)CancelSynchronousIo(m_worker.native_handle());
    m_worker.join();
    GUARDIAN_LOG_INFO("agent.ipc", "IPC server worker stopped");
}

void IpcServer::run() {
    using namespace std::chrono_literals;

    LocalSecurityDescriptor security;
    if (!security.valid()) {
        GUARDIAN_LOG_ERROR(
            "agent.ipc",
            "Failed to create Named Pipe security descriptor; "
            "win32_error=",
            security.error());
        return;
    }
    auto security_attributes = security.attributes();

    while (!m_stop_requested.load()) {
        const HANDLE pipe = CreateNamedPipeW(
            m_pipe_name.c_str(),
            PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE,
            PIPE_TYPE_BYTE |
                PIPE_READMODE_BYTE |
                PIPE_WAIT |
                PIPE_REJECT_REMOTE_CLIENTS,
            1,
            pipe_buffer_size,
            pipe_buffer_size,
            0,
            &security_attributes);

        if (pipe == INVALID_HANDLE_VALUE) {
            const DWORD error = GetLastError();
            GUARDIAN_LOG_ERROR(
                "agent.ipc",
                "CreateNamedPipeW failed; win32_error=",
                error);

            if (!m_stop_requested.load()) {
                std::this_thread::sleep_for(250ms);
            }
            continue;
        }

        GUARDIAN_LOG_DEBUG(
            "agent.ipc",
            "Waiting for a local IPC client");

        const BOOL connect_result = ConnectNamedPipe(pipe, nullptr);
        const DWORD connect_error =
            connect_result == FALSE ? GetLastError() : ERROR_SUCCESS;
        const bool connected =
            connect_result != FALSE || connect_error == ERROR_PIPE_CONNECTED;

        if (m_stop_requested.load()) {
            if (connected) {
                DisconnectNamedPipe(pipe);
            }
            CloseHandle(pipe);
            break;
        }

        if (!connected) {
            GUARDIAN_LOG_WARNING(
                "agent.ipc",
                "ConnectNamedPipe failed; win32_error=",
                connect_error);
            CloseHandle(pipe);
            continue;
        }

        GUARDIAN_LOG_INFO("agent.ipc", "Local IPC client connected");

        std::string receive_buffer;
        bool keep_connection = true;

        while (keep_connection && !m_stop_requested.load()) {
            char bytes[4096];
            DWORD bytes_read = 0;
            const BOOL success = ReadFile(
                pipe,
                bytes,
                sizeof(bytes),
                &bytes_read,
                nullptr);

            if (success == FALSE) {
                const DWORD error = GetLastError();
                if (error != ERROR_OPERATION_ABORTED ||
                    !m_stop_requested.load()) {
                    if (error == ERROR_HANDLE_EOF ||
                        error == ERROR_BROKEN_PIPE ||
                        error == ERROR_NO_DATA) {
                        GUARDIAN_LOG_INFO(
                            "agent.ipc",
                            "Local IPC client disconnected");
                    } else {
                        GUARDIAN_LOG_WARNING(
                            "agent.ipc",
                            "ReadFile failed; win32_error=",
                            error);
                    }
                }
                break;
            }

            if (bytes_read == 0) {
                GUARDIAN_LOG_INFO(
                    "agent.ipc",
                    "Local IPC client produced an empty read");
                break;
            }

            receive_buffer.append(bytes, bytes_read);

            while (keep_connection) {
                auto decoded = guardian::ipc::try_decode_frame(receive_buffer);
                if (decoded.status ==
                    guardian::ipc::FrameDecodeStatus::Incomplete) {
                    break;
                }

                if (decoded.status ==
                    guardian::ipc::FrameDecodeStatus::InvalidLength) {
                    GUARDIAN_LOG_WARNING(
                        "agent.ipc",
                        "Rejected IPC frame with an invalid payload length");
                    keep_connection = false;
                    break;
                }

                receive_buffer.erase(0, decoded.consumed_bytes);
                keep_connection = process_request(
                    decoded.payload,
                    pipe,
                    m_store,
                    m_stop_requested);
            }
        }

        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
        GUARDIAN_LOG_DEBUG(
            "agent.ipc",
            "Local IPC connection closed; waiting for the next client");
    }
}

}  // namespace guardian::agent
