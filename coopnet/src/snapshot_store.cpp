#include "coopnet/snapshot_store.h"

#include <utility>

namespace coopnet {

void SnapshotStore::clear() {
    tick_ = 0;
    lastProcessedInput_ = 0;
    entities_.clear();
    history_.clear();
}

const EntityState* SnapshotStore::find(std::uint32_t netId) const {
    const auto it = entities_.find(netId);
    return it == entities_.end() ? nullptr : &it->second;
}

const SnapshotStore::World* SnapshotStore::findBase(std::uint32_t tick) const {
    for (auto it = history_.rbegin(); it != history_.rend(); ++it) {
        if (it->first == tick) {
            return &it->second;
        }
    }
    return nullptr;
}

void SnapshotStore::remember(std::uint32_t tick, const World& world) {
    history_.emplace_back(tick, world);
    while (history_.size() > kHistory) {
        history_.pop_front();
    }
}

bool SnapshotStore::apply(const Snapshot& snapshot) {
    if (snapshot.baseTick == 0) {
        if (snapshot.tick < tick_) {
            return true;  // stale full snapshot: keep the newer world
        }
        World world;
        for (const auto& entity : snapshot.entities) {
            world[entity.netId] = entity;
        }
        entities_ = std::move(world);
        tick_ = snapshot.tick;
        lastProcessedInput_ = snapshot.lastProcessedInput;
        remember(tick_, entities_);
        return true;
    }

    const World* base = findBase(snapshot.baseTick);
    if (base == nullptr) {
        return false;
    }
    if (snapshot.tick <= tick_) {
        return true;  // already at this tick or newer
    }

    World world = *base;
    for (const auto& delta : snapshot.entities) {
        auto it = world.find(delta.netId);
        if (it == world.end()) {
            // A delta may introduce an entity it just spawned; fields missing
            // from the mask keep their defaults.
            world.emplace(delta.netId, delta);
            continue;
        }

        EntityState& target = it->second;
        const std::uint16_t mask = delta.present;
        if (mask & kFieldPosition) {
            target.position = delta.position;
        }
        if (mask & kFieldVelocity) {
            target.velocity = delta.velocity;
        }
        if (mask & kFieldAngles) {
            target.yaw = delta.yaw;
            target.pitch = delta.pitch;
        }
        if (mask & kFieldState) {
            target.state = delta.state;
            target.stateFlags = delta.stateFlags;
        }
        if (mask & kFieldHealth) {
            target.health = delta.health;
        }
        if (mask & kFieldWeapon) {
            target.weaponSlot = delta.weaponSlot;
            target.weaponState = delta.weaponState;
        }
        if (mask & kFieldAnimPhase) {
            target.animPhase = delta.animPhase;
        }
        if (mask & kFieldAiState) {
            target.aiState = delta.aiState;
        }
        if (mask & kFieldOwner) {
            target.ownerNetId = delta.ownerNetId;
        }
        target.present |= mask;
    }

    for (const auto netId : snapshot.removed) {
        world.erase(netId);
    }

    entities_ = std::move(world);
    tick_ = snapshot.tick;
    lastProcessedInput_ = snapshot.lastProcessedInput;
    remember(tick_, entities_);
    return true;
}

} // namespace coopnet