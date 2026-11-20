#include "coopnet/actor.h"

#include <cstdint>

#include "test_framework.h"

namespace {

coopnet::InputState inputAt(std::uint32_t tick) {
    coopnet::InputState input;
    input.tick = tick;
    input.moveY = 1.0f;
    input.viewYaw = 0.0f;
    return input;
}

}  // namespace

COOP_TEST(actor_applies_inputs_once_in_tick_order) {
    constexpr float kDt = 1.0f / 60.0f;
    coopnet::PlayerActor actor(1000);

    CHECK(actor.applyInput(inputAt(2), kDt));
    const float zAfterFirst = actor.state().position.z;
    CHECK(zAfterFirst > 0.0f);
    CHECK_EQ(actor.lastInputTick(), 2u);

    // Duplicate and stale ticks must not move the actor.
    CHECK(!actor.applyInput(inputAt(2), kDt));
    CHECK(!actor.applyInput(inputAt(1), kDt));
    CHECK_EQ(actor.state().position.z, zAfterFirst);
    CHECK_EQ(actor.lastInputTick(), 2u);

    CHECK(actor.applyInput(inputAt(3), kDt));
    CHECK(actor.state().position.z > zAfterFirst);
    CHECK_EQ(actor.lastInputTick(), 3u);
}

COOP_TEST(actor_entity_carries_motion_and_view) {
    coopnet::PlayerActor actor(1031);
    coopnet::InputState input = inputAt(7);
    input.viewYaw = 0.5f;
    input.viewPitch = -0.2f;
    input.weaponSlot = 2;
    CHECK(actor.applyInput(input, 1.0f / 60.0f));

    const coopnet::EntityState entity = actor.toEntity();
    CHECK_EQ(entity.netId, 1031u);
    CHECK((entity.present & coopnet::kFieldPosition) != 0);
    CHECK((entity.present & coopnet::kFieldVelocity) != 0);
    CHECK((entity.present & coopnet::kFieldAngles) != 0);
    CHECK((entity.present & coopnet::kFieldState) != 0);
    CHECK((entity.present & coopnet::kFieldWeapon) != 0);
    CHECK((entity.present & coopnet::kFieldOwner) != 0);
    CHECK_EQ(entity.position.z, actor.state().position.z);
    CHECK_EQ(entity.velocity.z, actor.state().velocity.z);
    CHECK_EQ(entity.yaw, 0.5f);
    CHECK_EQ(entity.pitch, -0.2f);
    CHECK_EQ(entity.weaponSlot, 2u);
    CHECK_EQ(entity.ownerNetId, 1031u);
}