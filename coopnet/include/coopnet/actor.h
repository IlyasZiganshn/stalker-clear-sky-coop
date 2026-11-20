#pragma once

#include <cstdint>

#include "coopnet/movement.h"
#include "coopnet/snapshot.h"

namespace coopnet {

// Server-side player simulation: applies C_INPUT exactly once and in tick
// order, so duplicated or reordered datagrams cannot move the actor twice.
class PlayerActor {
public:
    explicit PlayerActor(std::uint32_t netId, MovementConfig config = {});

    // Returns false for a tick that was already applied or is older than the
    // last one; the actor stays put in that case.
    bool applyInput(const InputState& input, float dtSeconds);

    std::uint32_t netId() const { return netId_; }
    std::uint32_t lastInputTick() const { return lastInputTick_; }
    const MoveState& state() const { return state_; }

    // Entity record carrying everything a client needs to draw and reconcile
    // this player: position, velocity, view angles, weapon and ownership.
    EntityState toEntity(std::uint8_t classId = 1) const;

private:
    std::uint32_t netId_;
    MovementConfig config_;
    MoveState state_;
    std::uint32_t lastInputTick_ = 0;
    float yaw_ = 0.0f;
    float pitch_ = 0.0f;
    std::uint8_t weaponSlot_ = 0;
};

} // namespace coopnet