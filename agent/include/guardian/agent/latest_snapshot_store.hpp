#pragma once

#include <guardian/model/identifiers.hpp>
#include <guardian/model/metrics.hpp>

#include <mutex>
#include <optional>
#include <string>

namespace guardian::agent {

struct LatestSnapshot {
    guardian::model::Timestamp collected_at;

    std::optional<guardian::model::CpuMetric> cpu;
    std::optional<guardian::model::MemoryMetric> memory;

    std::string collection_error;
    std::string submission_error;
};

class LatestSnapshotStore {
public:
    void update(LatestSnapshot snapshot);

    [[nodiscard]] std::optional<LatestSnapshot> get() const;

private:
    mutable std::mutex m_mutex;
    std::optional<LatestSnapshot> m_snapshot;
};

}  // namespace guardian::agent
