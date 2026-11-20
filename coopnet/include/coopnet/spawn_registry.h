#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "coopnet/spawn.h"

namespace coopnet {

// Bookkeeping of the spawn blobs a host relays. A peer can publish a new blob
// for the same net_id (it reloaded the level); the registry reports the change
// so the host can tear down the old doll and spawn the new one.
class SpawnRegistry {
public:
    enum class Change {
        Added,
        Unchanged,
        Replaced,
    };

    // Empty blobs and net_id 0 are ignored (Unchanged).
    Change observe(const Spawn& spawn);

    bool remove(std::uint32_t netId);
    const Spawn* find(std::uint32_t netId) const;

    std::size_t size() const { return spawns_.size(); }
    void clear() { spawns_.clear(); }
    const std::vector<Spawn>& all() const { return spawns_; }

private:
    std::vector<Spawn> spawns_;
};

} // namespace coopnet