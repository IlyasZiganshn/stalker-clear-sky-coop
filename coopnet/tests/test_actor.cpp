#include "coopnet/actor.h"

#include <cstddef>
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

COOP_TEST(actor_buffers_reordered_inputs_behind_a_gap) {
    constexpr float kDt = 1.0f / 60.0f;
    coopnet::PlayerActor actor(1000);
    coopnet::PlayerActor reference(1000);

    CHECK(actor.applyInput(inputAt(1), kDt));
    CHECK(reference.applyInput(inputAt(1), kDt));

    // Tick 2 is still in flight: tick 3 must wait instead of skipping a step.
    CHECK(!actor.applyInput(inputAt(3), kDt));
    CHECK_EQ(actor.lastInputTick(), 1u);
    CHECK_EQ(actor.state().position.z, reference.state().position.z);
    CHECK_EQ(actor.bufferedInputs(), std::size_t{1});

    // The missing tick arrives: both 2 and the buffered 3 apply in order.
    CHECK(actor.applyInput(inputAt(2), kDt));
    CHECK(reference.applyInput(inputAt(2), kDt));
    CHECK(reference.applyInput(inputAt(3), kDt));
    CHECK_EQ(actor.lastInputTick(), 3u);
    CHECK_EQ(actor.bufferedInputs(), std::size_t{0});
    CHECK_EQ(actor.state().position.z, reference.state().position.z);

    // A duplicate behind the gap must not corrupt the buffer.
    CHECK(!actor.applyInput(inputAt(2), kDt));
    CHECK_EQ(actor.state().position.z, reference.state().position.z);
}

COOP_TEST(actor_skips_permanently_lost_tick) {
    constexpr float kDt = 1.0f / 60.0f;
    coopnet::PlayerActor actor(1000);
    CHECK(actor.applyInput(inputAt(1), kDt));

    const std::uint32_t lastTick =
        static_cast<std::uint32_t>(1 + coopnet::PlayerActor::kMaxBufferedInputs + 2);
    for (std::uint32_t tick = 3; tick <= lastTick; ++tick) {
        actor.applyInput(inputAt(tick), kDt);
    }

    // The gap never healed: the actor moves on rather than freezing forever.
    CHECK_EQ(actor.lastInputTick(), lastTick);
    CHECK_EQ(actor.bufferedInputs(), std::size_t{0});
    CHECK(actor.state().position.z > 0.0f);
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