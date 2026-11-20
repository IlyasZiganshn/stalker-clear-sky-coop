#pragma once

#include <cstdint>
#include <map>

#include "coopnet/snapshot.h"

namespace coopnet {

// Client-side world state: keeps the last applied snapshot and merges deltas
// into it. A full snapshot (base_tick = 0) replaces the world; a delta must
// name the tick currently held, otherwise it is rejected and the caller should
// request a full snapshot.
class SnapshotStore {
public:
    bool apply(const Snapshot& snapshot);
    void clear();

    std::uint32_t tick() const { return tick_; }
    bool empty() const { return entities_.empty(); }
    std::size_t size() const { return entities_.size(); }
    const EntityState* find(std::uint32_t netId) const;
    const std::map<std::uint32_t, EntityState>& entities() const {
        return entities_;
    }

private:
    std::uint32_t tick_ = 0;
    std::map<std::uint32_t, EntityState> entities_;
};

}  // namespace coopnet