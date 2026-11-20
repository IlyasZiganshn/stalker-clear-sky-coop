#include "coopnet/baseline.h"

namespace coopnet {

bool SnapshotBaselines::record(Snapshot snapshot) {
    if (snapshot.baseTick != 0) {
        return false;
    }
    if (!snapshots_.empty() && snapshot.tick <= snapshots_.back().tick) {
        return false;
    }
    snapshots_.push_back(std::move(snapshot));
    while (snapshots_.size() > kMaxSnapshots) {
        snapshots_.pop_front();
    }
    return true;
}

const Snapshot* SnapshotBaselines::find(std::uint32_t tick) const {
    for (const auto& snapshot : snapshots_) {
        if (snapshot.tick == tick) {
            return &snapshot;
        }
    }
    return nullptr;
}

}  // namespace coopnet