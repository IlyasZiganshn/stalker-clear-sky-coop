#include "coopnet/interp.h"

#include <cstddef>
#include <vector>

#include "test_framework.h"

using coopnet::EntityState;
using coopnet::InterpolationBuffer;
using coopnet::kFieldAngles;
using coopnet::kFieldPosition;
using coopnet::kFieldState;

namespace {

constexpr float kPi = 3.14159265358979323846f;

EntityState makeEntity(std::uint32_t netId, float x, float yaw = 0.0f,
                       float pitch = 0.0f) {
    EntityState entity;
    entity.netId = netId;
    entity.present = kFieldPosition | kFieldAngles;
    entity.position = {x, 0.0f, 0.0f};
    entity.yaw = yaw;
    entity.pitch = pitch;
    return entity;
}

bool near(float a, float b, float tolerance = 0.001f) {
    return (a > b ? a - b : b - a) <= tolerance;
}

}  // namespace

COOP_TEST(interp_blends_position_and_angles) {
    InterpolationBuffer buffer;
    buffer.push(1, 0, {makeEntity(1, 0.0f, 0.0f, 0.0f)});
    buffer.push(2, 100, {makeEntity(1, 10.0f, kPi / 2.0f, 0.2f)});

    std::vector<EntityState> out;
    buffer.sample(100, 50, out);  // render time = 50 ms

    CHECK_EQ(out.size(), std::size_t{1});
    CHECK(near(out[0].position.x, 5.0f));
    CHECK(near(out[0].yaw, kPi / 4.0f));
    CHECK(near(out[0].pitch, 0.1f));
}

COOP_TEST(interp_takes_shortest_arc_for_yaw) {
    InterpolationBuffer buffer;
    // 350 degrees to 10 degrees must go through 0, not the long way round.
    buffer.push(1, 0, {makeEntity(1, 0.0f, 350.0f * kPi / 180.0f)});
    buffer.push(2, 100, {makeEntity(1, 0.0f, 10.0f * kPi / 180.0f)});

    std::vector<EntityState> out;
    buffer.sample(100, 50, out);
    CHECK_EQ(out.size(), std::size_t{1});
    CHECK(near(coopnet::wrapAngle(out[0].yaw), 0.0f));
}

COOP_TEST(interp_holds_last_sample_without_extrapolation) {
    InterpolationBuffer buffer;
    buffer.push(1, 0, {makeEntity(1, 0.0f)});
    buffer.push(2, 100, {makeEntity(1, 10.0f)});

    std::vector<EntityState> out;
    buffer.sample(5000, 100, out);
    CHECK_EQ(out.size(), std::size_t{1});
    CHECK(near(out[0].position.x, 10.0f));
}

COOP_TEST(interp_clamps_before_first_sample) {
    InterpolationBuffer buffer;
    std::vector<EntityState> out;
    buffer.sample(100, 50, out);
    CHECK(out.empty());

    buffer.push(1, 1000, {makeEntity(1, 3.0f)});
    buffer.sample(100, 50, out);
    CHECK_EQ(out.size(), std::size_t{1});
    CHECK(near(out[0].position.x, 3.0f));
}

COOP_TEST(interp_uses_newer_sample_for_lifecycle) {
    InterpolationBuffer buffer;
    buffer.push(1, 0, {makeEntity(1, 0.0f)});
    buffer.push(2, 100, {makeEntity(2, 7.0f)});

    std::vector<EntityState> out;
    buffer.sample(150, 25, out);  // render time = 125 ms: past the newer sample
    CHECK_EQ(out.size(), std::size_t{1});
    CHECK_EQ(out[0].netId, 2u);
    CHECK(near(out[0].position.x, 7.0f));
}

COOP_TEST(interp_keeps_discrete_fields_from_the_older_sample) {
    InterpolationBuffer buffer;

    EntityState older = makeEntity(1, 0.0f);
    older.present |= kFieldState;
    older.state = 0;

    EntityState newer = makeEntity(1, 10.0f);
    newer.present |= kFieldState;
    newer.state = 1;

    buffer.push(1, 0, {older});
    buffer.push(2, 100, {newer});

    std::vector<EntityState> out;
    buffer.sample(100, 50, out);
    CHECK_EQ(out.size(), std::size_t{1});
    CHECK_EQ(out[0].state, std::uint8_t{0});
}

COOP_TEST(interp_ignores_out_of_order_samples_and_is_bounded) {
    InterpolationBuffer buffer;
    buffer.push(2, 200, {makeEntity(1, 20.0f)});
    buffer.push(1, 100, {makeEntity(1, 10.0f)});  // older: ignored
    CHECK_EQ(buffer.size(), std::size_t{1});

    for (std::size_t i = 0; i < InterpolationBuffer::kMaxSamples + 10; ++i) {
        buffer.push(static_cast<std::uint32_t>(10 + i),
                    static_cast<std::uint32_t>(300 + i * 10),
                    {makeEntity(1, static_cast<float>(i))});
    }
    CHECK_EQ(buffer.size(), InterpolationBuffer::kMaxSamples);
}