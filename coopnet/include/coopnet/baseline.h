#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>

#include "coopnet/snapshot.h"

namespace coopnet {

// Per-client history of full server snapshots, used as delta bases. The client
// confirms the tick it holds (`C_INPUT.ack_snapshot`); a base that was never
// recorded or has been evicted means the next snapshot must be full.
class SnapshotBaselines {
public:
    static constexpr std::size_t kMaxSnapshots = 64;

    // Stores a full world state (baseTick == 0). Ticks must grow strictly; a
    // delta or a duplicate tick is rejected.
    bool record(Snapshot snapshot);

    // Snapshot with exactly `tick`, or nullptr if it was never recorded or has
    // been evicted.
    const Snapshot* find(std::uint32_t tick) const;

    std::size_t size() const { return snapshots_.size(); }
    void clear() { snapshots_.clear(); }

private:
    std::deque<Snapshot> snapshots_;
};

}  // namespace coopnet