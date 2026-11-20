#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>

#include "coopnet/snapshot.h"

namespace coopnet {

// Client-side world state. A full snapshot (base_tick = 0) replaces the world;
// a delta is applied to the newest retained state matching its base_tick. The
// last states are kept so a delta built from a slightly older acknowledged tick
// still applies after a lost snapshot, without a full resync. A delta whose
// base is no longer retained is rejected and the caller should ask for a full
// snapshot.
class SnapshotStore {
public:
    static constexpr std::size_t kHistory = 16;

    bool apply(const Snapshot& snapshot);
    void clear();

    std::uint32_t tick() const { return tick_; }
    std::uint32_t lastProcessedInput() const { return lastProcessedInput_; }
    bool empty() const { return entities_.empty(); }
    std::size_t size() const { return entities_.size(); }
    const EntityState* find(std::uint32_t netId) const;
    const std::map<std::uint32_t, EntityState>& entities() const {
        return entities_;
    }

private:
    using World = std::map<std::uint32_t, EntityState>;

    const World* findBase(std::uint32_t tick) const;
    void remember(std::uint32_t tick, const World& world);

    std::uint32_t tick_ = 0;
    std::uint32_t lastProcessedInput_ = 0;
    World entities_;
    std::deque<std::pair<std::uint32_t, World>> history_;
};

} // namespace coopnet