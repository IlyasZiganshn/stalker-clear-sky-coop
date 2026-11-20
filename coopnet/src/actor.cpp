#include "coopnet/actor.h"

namespace coopnet {

PlayerActor::PlayerActor(std::uint32_t netId, MovementConfig config)
    : netId_(netId), config_(config) {}

bool PlayerActor::applyInput(const InputState& input, float dtSeconds) {
    if (lastInputTick_ != 0 && input.tick <= lastInputTick_) {
        return false;
    }
    stepMovement(state_, input, config_, dtSeconds);
    lastInputTick_ = input.tick;
    yaw_ = input.viewYaw;
    pitch_ = input.viewPitch;
    weaponSlot_ = input.weaponSlot;
    return true;
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