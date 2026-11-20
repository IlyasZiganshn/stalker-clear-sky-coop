#include "coopnet/delta.h"

#include <map>
#include <set>

namespace coopnet {

namespace {

bool present(std::uint16_t mask, std::uint16_t field) {
    return (mask & field) != 0;
}

std::uint16_t changedFields(const EntityState& base,
                            const EntityState& current) {
    std::uint16_t changed = 0;

    if (present(current.present, kFieldPosition) &&
        (!present(base.present, kFieldPosition) ||
         base.position.x != current.position.x ||
         base.position.y != current.position.y ||
         base.position.z != current.position.z)) {
        changed |= kFieldPosition;
    }
    if (present(current.present, kFieldVelocity) &&
        (!present(base.present, kFieldVelocity) ||
         base.velocity.x != current.velocity.x ||
         base.velocity.y != current.velocity.y ||
         base.velocity.z != current.velocity.z)) {
        changed |= kFieldVelocity;
    }
    if (present(current.present, kFieldAngles) &&
        (!present(base.present, kFieldAngles) || base.yaw != current.yaw ||
         base.pitch != current.pitch)) {
        changed |= kFieldAngles;
    }
    if (present(current.present, kFieldState) &&
        (!present(base.present, kFieldState) || base.state != current.state ||
         base.stateFlags != current.stateFlags)) {
        changed |= kFieldState;
    }
    if (present(current.present, kFieldHealth) &&
        (!present(base.present, kFieldHealth) ||
         base.health != current.health)) {
        changed |= kFieldHealth;
    }
    if (present(current.present, kFieldWeapon) &&
        (!present(base.present, kFieldWeapon) ||
         base.weaponSlot != current.weaponSlot ||
         base.weaponState != current.weaponState)) {
        changed |= kFieldWeapon;
    }
    if (present(current.present, kFieldAnimPhase) &&
        (!present(base.present, kFieldAnimPhase) ||
         base.animPhase != current.animPhase)) {
        changed |= kFieldAnimPhase;
    }
    if (present(current.present, kFieldAiState) &&
        (!present(base.present, kFieldAiState) ||
         base.aiState != current.aiState)) {
        changed |= kFieldAiState;
    }
    if (present(current.present, kFieldOwner) &&
        (!present(base.present, kFieldOwner) ||
         base.ownerNetId != current.ownerNetId)) {
        changed |= kFieldOwner;
    }
    return changed;
}

}  // namespace

Snapshot SnapshotDelta::makeDelta(const Snapshot& base,
                                  const Snapshot& current) {
    Snapshot delta;
    delta.tick = current.tick;
    delta.baseTick = base.tick;

    std::map<std::uint32_t, const EntityState*> previous;
    for (const auto& entity : base.entities) {
        previous.emplace(entity.netId, &entity);
    }

    std::set<std::uint32_t> alive;
    for (const auto& entity : current.entities) {
        alive.insert(entity.netId);
        const auto it = previous.find(entity.netId);
        if (it == previous.end()) {
            delta.entities.push_back(entity);
            continue;
        }
        const std::uint16_t mask = changedFields(*it->second, entity);
        if (mask != 0) {
            EntityState record = entity;
            record.present = mask;
            delta.entities.push_back(std::move(record));
        }
    }

    for (const auto& entity : base.entities) {
        if (alive.find(entity.netId) == alive.end()) {
            delta.removed.push_back(entity.netId);
        }
    }

    return delta;
}

}  // namespace coopnet