#include <guardian/agent/latest_snapshot_store.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <thread>

namespace {

int failure_count = 0;

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        ++failure_count;
    }
}

guardian::agent::LatestSnapshot make_snapshot(std::int64_t sequence) {
    guardian::agent::LatestSnapshot snapshot{};
    snapshot.collected_at = guardian::model::Timestamp{
        std::chrono::milliseconds{sequence}};
    snapshot.cpu = guardian::model::CpuMetric{
        static_cast<double>(sequence % 101)};
    snapshot.memory = guardian::model::MemoryMetric{
        static_cast<std::uint64_t>(sequence),
        static_cast<std::uint64_t>(sequence / 2)};
    snapshot.collection_error = "collection-" + std::to_string(sequence);
    snapshot.submission_error = "submission-" + std::to_string(sequence);
    return snapshot;
}

bool is_consistent(const guardian::agent::LatestSnapshot& snapshot) {
    const auto sequence = snapshot.collected_at.time_since_epoch().count();
    return snapshot.cpu.has_value() &&
           snapshot.memory.has_value() &&
           snapshot.cpu->usage_percent ==
               static_cast<double>(sequence % 101) &&
           snapshot.memory->total_bytes ==
               static_cast<std::uint64_t>(sequence) &&
           snapshot.memory->available_bytes ==
               static_cast<std::uint64_t>(sequence / 2) &&
           snapshot.collection_error ==
               "collection-" + std::to_string(sequence) &&
           snapshot.submission_error ==
               "submission-" + std::to_string(sequence);
}

}  // namespace

int main() {
    guardian::agent::LatestSnapshotStore store;

    expect(!store.get().has_value(),
           "new store has no snapshot");

    store.update(make_snapshot(42));
    const auto first = store.get();
    expect(first.has_value(),
           "update publishes a snapshot");
    if (first) {
        expect(is_consistent(*first),
               "get returns every field from the published snapshot");
    }

    auto returned_copy = store.get();
    if (returned_copy) {
        returned_copy->collection_error = "changed outside store";
        returned_copy->cpu.reset();
    }

    const auto unchanged = store.get();
    expect(unchanged.has_value() && is_consistent(*unchanged),
           "get returns a copy independent from stored state");

    guardian::agent::LatestSnapshot partial{};
    partial.collected_at = guardian::model::Timestamp{
        std::chrono::milliseconds{100}};
    partial.memory = guardian::model::MemoryMetric{8192, 2048};
    partial.collection_error = "CPU collection failed";
    store.update(partial);

    const auto replacement = store.get();
    expect(replacement.has_value(),
           "second update keeps a snapshot available");
    if (replacement) {
        expect(!replacement->cpu.has_value(),
               "replacement removes a missing CPU metric");
        expect(replacement->memory.has_value() &&
                   replacement->memory->total_bytes == 8192 &&
                   replacement->memory->available_bytes == 2048,
               "replacement preserves a partial memory metric");
        expect(replacement->collection_error == "CPU collection failed",
               "replacement preserves the collection error");
        expect(replacement->submission_error.empty(),
               "replacement does not retain an old submission error");
    }

    store.update(make_snapshot(1));
    std::atomic_bool writer_done{false};
    std::atomic_bool inconsistent_read{false};

    std::thread writer{[&] {
        for (std::int64_t sequence = 2; sequence <= 5000; ++sequence) {
            store.update(make_snapshot(sequence));
        }
        writer_done = true;
    }};

    std::thread reader{[&] {
        while (!writer_done.load()) {
            const auto current = store.get();
            if (!current || !is_consistent(*current)) {
                inconsistent_read = true;
                return;
            }
        }
    }};

    writer.join();
    reader.join();

    expect(!inconsistent_read.load(),
           "concurrent get never observes a partially updated snapshot");
    const auto final_snapshot = store.get();
    expect(final_snapshot.has_value() && is_consistent(*final_snapshot),
           "concurrent updates leave a valid final snapshot");

    if (failure_count != 0) {
        std::cerr << failure_count << " test(s) failed\n";
        return 1;
    }

    return 0;
}
