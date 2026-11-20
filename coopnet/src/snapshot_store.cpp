#include "coopnet/snapshot_store.h"

namespace coopnet {

void SnapshotStore::clear() {
    tick_ = 0;
    lastProcessedInput_ = 0;
    entities_.clear();
}

const EntityState* SnapshotStore::find(std::uint32_t netId) const {
    const auto it = entities_.find(netId);
    return it == entities_.end() ? nullptr : &it->second;
}

bool SnapshotStore::apply(const Snapshot& snapshot) {
    if (snapshot.baseTick == 0) {
        entities_.clear();
        for (const auto& entity : snapshot.entities) {
            entities_[entity.netId] = entity;
        }
        tick_ = snapshot.tick;
        lastProcessedInput_ = snapshot.lastProcessedInput;
        return true;
    }

    if (snapshot.baseTick != tick_) {
        return false;
    }

    for (const auto& delta : snapshot.entities) {
        auto it = entities_.find(delta.netId);
        if (it == entities_.end()) {
            // A delta may introduce an entity it just spawned; fields missing
            // from the mask keep their defaults.
            entities_.emplace(delta.netId, delta);
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
        entities_.erase(netId);
    }
    tick_ = snapshot.tick;
    lastProcessedInput_ = snapshot.lastProcessedInput;
    return true;
}

}  // namespace coopnet