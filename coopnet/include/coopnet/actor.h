#pragma once

#include <cstddef>
#include <cstdint>
#include <map>

#include "coopnet/movement.h"
#include "coopnet/snapshot.h"

namespace coopnet {

// Server-side player simulation: applies C_INPUT exactly once and in tick
// order, so duplicated or reordered datagrams cannot move the actor twice.
// A short gap is buffered: inputs that arrive out of order wait for the
// missing tick instead of being applied on top of a skipped step. If the gap
// does not heal (a tick is lost for good), the actor drops it and continues,
// so the other clients still see movement.
class PlayerActor {
public:
    static constexpr std::size_t kMaxBufferedInputs = 32;

    explicit PlayerActor(std::uint32_t netId, MovementConfig config = {});

    // Returns true only for an input applied immediately (the next tick in
    // order). Duplicates, stale ticks and inputs buffered behind a gap return
    // false and leave the state untouched.
    bool applyInput(const InputState& input, float dtSeconds);

    std::uint32_t netId() const { return netId_; }
    std::uint32_t lastInputTick() const { return lastInputTick_; }
    const MoveState& state() const { return state_; }
    std::size_t bufferedInputs() const { return buffered_.size(); }

    // Entity record carrying everything a client needs to draw and reconcile
    // this player: position, velocity, view angles, weapon and ownership.
    EntityState toEntity(std::uint8_t classId = 1) const;

private:
    void applyOne(const InputState& input, float dtSeconds);
    void drain(float dtSeconds);

    std::uint32_t netId_;
    MovementConfig config_;
    MoveState state_;
    std::uint32_t lastInputTick_ = 0;
    float yaw_ = 0.0f;
    float pitch_ = 0.0f;
    std::uint8_t weaponSlot_ = 0;
    std::map<std::uint32_t, InputState> buffered_;
};

} // namespace coopnet