#include "coopnet/player_interp.h"

#include <cstddef>

#include "coopnet/interp.h"
#include "test_framework.h"

using coopnet::PlayerInterpolator;
using coopnet::PlayerState;

namespace {

constexpr float kPi = 3.14159265358979323846f;

PlayerState player(std::uint32_t netId, float x, float yaw = 0.0f,
                   std::uint16_t bodyState = 0) {
    PlayerState state;
    state.netId = netId;
    state.x = x;
    state.yaw = yaw;
    state.bodyState = bodyState;
    return state;
}

bool near(float a, float b, float tolerance = 0.001f) {
    return (a > b ? a - b : b - a) <= tolerance;
}

} // namespace

COOP_TEST(player_interp_blends_position_and_yaw) {
    PlayerInterpolator interp;
    interp.push(player(7, 0.0f, 0.0f), 1000);
    interp.push(player(7, 10.0f, kPi / 2.0f), 1050);

    // Render time = now - 25 ms: halfway between the two samples.
    PlayerState out[4];
    const std::uint32_t count = interp.sample(1050, 25, 2000, out, 4);
    CHECK_EQ(count, 1u);
    CHECK(near(out[0].x, 5.0f));
    CHECK(near(out[0].yaw, kPi / 4.0f));
}

COOP_TEST(player_interp_holds_discrete_body_state) {
    PlayerInterpolator interp;
    interp.push(player(1, 0.0f, 0.0f, 2), 0);
    interp.push(player(1, 10.0f, 0.0f, 4), 100);

    PlayerState out[1];
    // Halfway between the samples the transform is blended, the body state is
    // not: it still belongs to the older sample.
    CHECK_EQ(interp.sample(50, 0, 2000, out, 1), 1u);
    CHECK_EQ(out[0].bodyState, std::uint16_t{2});

    // Once the render time reaches the newer sample, its body state is used.
    CHECK_EQ(interp.sample(150, 0, 2000, out, 1), 1u);
    CHECK_EQ(out[0].bodyState, std::uint16_t{4});
}

COOP_TEST(player_interp_takes_shortest_arc) {
    PlayerInterpolator interp;
    interp.push(player(1, 0.0f, 350.0f * kPi / 180.0f), 0);
    interp.push(player(1, 0.0f, 10.0f * kPi / 180.0f), 100);

    PlayerState out[1];
    const std::uint32_t count = interp.sample(50, 0, 2000, out, 1);
    CHECK_EQ(count, 1u);
    CHECK(near(coopnet::wrapAngle(out[0].yaw), 0.0f));
}

COOP_TEST(player_interp_clamps_without_extrapolation) {
    PlayerInterpolator interp;
    interp.push(player(1, 0.0f), 0);
    interp.push(player(1, 10.0f), 100);

    PlayerState out[1];
    // Render time before the oldest sample: hold it, do not look ahead.
    CHECK_EQ(interp.sample(50, 100, 2000, out, 1), 1u);
    CHECK(near(out[0].x, 0.0f));

    // Render time past the newest sample: hold it too.
    CHECK_EQ(interp.sample(5000, 100, 10000, out, 1), 1u);
    CHECK(near(out[0].x, 10.0f));
}

COOP_TEST(player_interp_requires_a_previous_sample) {
    PlayerInterpolator interp;
    interp.push(player(1, 4.0f), 1000);

    PlayerState out[1];
    CHECK_EQ(interp.sample(1010, 100, 2000, out, 1), 1u);
    CHECK(near(out[0].x, 4.0f));
}

COOP_TEST(player_interp_ignores_out_of_order_and_forgets) {
    PlayerInterpolator interp;
    interp.push(player(1, 10.0f), 200);
    interp.push(player(1, 5.0f), 100); // older: ignored
    CHECK_EQ(interp.size(), std::size_t{1});

    PlayerState out[1];
    CHECK_EQ(interp.sample(200, 0, 2000, out, 1), 1u);
    CHECK(near(out[0].x, 10.0f));

    interp.forget(1);
    CHECK_EQ(interp.sample(200, 0, 2000, out, 1), 0u);
}

COOP_TEST(player_interp_drops_stale_tracks_and_caps_the_table) {
    PlayerInterpolator interp;
    interp.push(player(1, 1.0f), 0);
    PlayerState out[1];
    CHECK_EQ(interp.sample(1000, 0, 500, out, 1), 0u);

    CHECK_EQ(interp.sample(1000, 0, 2000, out, 1), 1u);

    for (std::uint32_t i = 0; i < PlayerInterpolator::kMaxTracks + 4; ++i) {
        interp.push(player(100 + i, static_cast<float>(i)), i * 10);
    }
    CHECK_EQ(interp.size(), PlayerInterpolator::kMaxTracks);
}