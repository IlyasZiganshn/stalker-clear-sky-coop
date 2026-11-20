#include "coopnet/spawn_registry.h"

namespace coopnet {

SpawnRegistry::Change SpawnRegistry::observe(const Spawn& spawn) {
    if (spawn.netId == 0 || spawn.data.empty()) {
        return Change::Unchanged;
    }

    for (auto& stored : spawns_) {
        if (stored.netId != spawn.netId) {
            continue;
        }
        if (stored.data == spawn.data) {
            return Change::Unchanged;
        }
        stored = spawn;
        return Change::Replaced;
    }

    spawns_.push_back(spawn);
    return Change::Added;
}

bool SpawnRegistry::remove(std::uint32_t netId) {
    for (auto it = spawns_.begin(); it != spawns_.end(); ++it) {
        if (it->netId == netId) {
            spawns_.erase(it);
            return true;
        }
    }
    return false;
}

const Spawn* SpawnRegistry::find(std::uint32_t netId) const {
    for (const auto& spawn : spawns_) {
        if (spawn.netId == netId) {
            return &spawn;
        }
    }
    return nullptr;
}

} // namespace coopnet