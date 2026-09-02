#pragma once

#include <guardian/agent/latest_snapshot_store.hpp>

#include <atomic>
#include <string>
#include <thread>

namespace guardian::agent {

class IpcServer {
public:
    explicit IpcServer(
        const LatestSnapshotStore& store,
        std::wstring pipe_name = LR"(\\.\pipe\ai_pc_guardian_v1)");

    ~IpcServer();

    void start();

    void stop();

    IpcServer& operator=(const IpcServer&) = delete;

    IpcServer(const IpcServer&) = delete;

private:
    const LatestSnapshotStore& m_store;
    std::wstring m_pipe_name;
    std::atomic<bool> m_stop_requested{false};
    std::thread m_worker;

    void run();
};

}  // namespace guardian::agent
