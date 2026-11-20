#include "coopnet/baseline.h"

#include <cstddef>

#include "test_framework.h"

using coopnet::Snapshot;
using coopnet::SnapshotBaselines;

namespace {

Snapshot fullSnapshot(std::uint32_t tick) {
    Snapshot snapshot;
    snapshot.tick = tick;
    snapshot.baseTick = 0;
    return snapshot;
}

}  // namespace

COOP_TEST(baseline_finds_recorded_snapshots) {
    SnapshotBaselines baselines;
    CHECK(baselines.record(fullSnapshot(10)));
    CHECK(baselines.record(fullSnapshot(11)));

    const Snapshot* found = baselines.find(10);
    CHECK(found != nullptr);
    CHECK_EQ(found->tick, 10u);

    CHECK(baselines.find(11) != nullptr);
    CHECK(baselines.find(9) == nullptr);
    CHECK(baselines.find(12) == nullptr);
}

COOP_TEST(baseline_rejects_deltas_and_duplicates) {
    SnapshotBaselines baselines;

    Snapshot delta = fullSnapshot(5);
    delta.baseTick = 4;
    CHECK(!baselines.record(delta));
    CHECK(baselines.size() == 0);

    CHECK(baselines.record(fullSnapshot(5)));
    CHECK(!baselines.record(fullSnapshot(5)));  // duplicate tick
    CHECK(!baselines.record(fullSnapshot(4)));  // older tick
    CHECK_EQ(baselines.size(), std::size_t{1});
}

COOP_TEST(baseline_evicts_oldest_and_reports_misses) {
    SnapshotBaselines baselines;
    for (std::size_t i = 0; i < SnapshotBaselines::kMaxSnapshots + 5; ++i) {
        CHECK(baselines.record(fullSnapshot(static_cast<std::uint32_t>(i + 1))));
    }
    CHECK_EQ(baselines.size(), SnapshotBaselines::kMaxSnapshots);
    CHECK(baselines.find(1) == nullptr);  // evicted: caller must send a full one
    CHECK(baselines.find(6) != nullptr);
    CHECK(baselines.find(static_cast<std::uint32_t>(
              SnapshotBaselines::kMaxSnapshots + 5)) != nullptr);

    baselines.clear();
    CHECK(baselines.size() == 0);
    CHECK(baselines.find(6) == nullptr);
}