#include <guardian/agent/latest_snapshot_store.hpp>

#include <utility>

namespace guardian::agent {

void LatestSnapshotStore::update(LatestSnapshot snapshot) {
    std::lock_guard<std::mutex> lock{m_mutex};
    m_snapshot = std::move(snapshot);
}

std::optional<LatestSnapshot> LatestSnapshotStore::get() const {
    std::lock_guard<std::mutex> lock{m_mutex};

    return m_snapshot;
}

}  // namespace guardian::agent
