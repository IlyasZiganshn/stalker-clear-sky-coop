#include "coopnet/actor.h"

namespace coopnet {

PlayerActor::PlayerActor(std::uint32_t netId, MovementConfig config)
    : netId_(netId), config_(config) {}

namespace {

void setView(InputState const& input, float& yaw, float& pitch,
             std::uint8_t& weaponSlot) {
    yaw = input.viewYaw;
    pitch = input.viewPitch;
    weaponSlot = input.weaponSlot;
}

}  // namespace

void PlayerActor::applyOne(const InputState& input, float dtSeconds) {
    stepMovement(state_, input, config_, dtSeconds);
    lastInputTick_ = input.tick;
    setView(input, yaw_, pitch_, weaponSlot_);
}

void PlayerActor::drain(float dtSeconds) {
    for (;;) {
        auto it = buffered_.begin();
        if (it == buffered_.end() || it->first != lastInputTick_ + 1) {
            break;
        }
        const InputState input = it->second;
        buffered_.erase(it);
        applyOne(input, dtSeconds);
    }
}

bool PlayerActor::applyInput(const InputState& input, float dtSeconds) {
    if (lastInputTick_ == 0) {
        applyOne(input, dtSeconds);
        return true;
    }
    if (input.tick <= lastInputTick_) {
        return false;
    }
    if (input.tick == lastInputTick_ + 1) {
        applyOne(input, dtSeconds);
        drain(dtSeconds);
        return true;
    }

    buffered_[input.tick] = input;
    if (buffered_.size() > kMaxBufferedInputs) {
        // A tick was lost for good: skip over the gap and keep going from the
        // earliest buffered input. Clients see one larger correction instead
        // of a frozen actor.
        lastInputTick_ = buffered_.begin()->first - 1;
        drain(dtSeconds);
    }
    return false;
}

EntityState PlayerActor::toEntity(std::uint8_t classId) const {
    EntityState entity;
    entity.netId = netId_;
    entity.classId = classId;
    entity.present = kFieldPosition | kFieldVelocity | kFieldAngles |
                     kFieldState | kFieldWeapon | kFieldOwner;
    entity.position = state_.position;
    entity.velocity = state_.velocity;
    entity.yaw = yaw_;
    entity.pitch = pitch_;
    entity.stateFlags = state_.onGround ? 1 : 0;
    entity.weaponSlot = weaponSlot_;
    entity.ownerNetId = netId_;
    return entity;
}

} // namespace coopnet