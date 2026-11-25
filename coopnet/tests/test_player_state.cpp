#include "coopnet/player_state.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "test_framework.h"

namespace {

coopnet::PlayerState sample() {
    coopnet::PlayerState state;
    state.netId = 1001;
    state.x = 12.5f;
    state.y = -3.25f;
    state.z = 87.125f;
    state.yaw = 1.5f;
    state.pitch = -0.35f;
    state.bodyState = 0x1234;
    return state;
}

} // namespace

COOP_TEST(player_state_roundtrip) {
    const coopnet::PlayerState original = sample();
    std::vector<std::uint8_t> wire;
    CHECK(coopnet::encodePlayerState(original, wire));
    CHECK_EQ(wire.size(), coopnet::kPlayerStateSize);

    coopnet::PlayerState decoded;
    CHECK(coopnet::decodePlayerState(wire.data(), wire.size(), decoded));
    CHECK_EQ(decoded.netId, original.netId);
    CHECK(decoded.x == original.x);
    CHECK(decoded.y == original.y);
    CHECK(decoded.z == original.z);
    CHECK(decoded.yaw == original.yaw);
    CHECK(decoded.pitch == original.pitch);
    CHECK_EQ(decoded.bodyState, original.bodyState);
}

COOP_TEST(player_state_rejects_bad_values_on_encode) {
    std::vector<std::uint8_t> wire;

    coopnet::PlayerState noId = sample();
    noId.netId = 0;
    CHECK(!coopnet::encodePlayerState(noId, wire));

    coopnet::PlayerState nan = sample();
    nan.x = std::numeric_limits<float>::quiet_NaN();
    CHECK(!coopnet::encodePlayerState(nan, wire));

    coopnet::PlayerState inf = sample();
    inf.yaw = std::numeric_limits<float>::infinity();
    CHECK(!coopnet::encodePlayerState(inf, wire));
}

COOP_TEST(player_state_rejects_malformed_input) {
    const coopnet::PlayerState original = sample();
    std::vector<std::uint8_t> wire;
    CHECK(coopnet::encodePlayerState(original, wire));

    coopnet::PlayerState decoded;
    CHECK(!coopnet::decodePlayerState(nullptr, wire.size(), decoded));
    CHECK(!coopnet::decodePlayerState(wire.data(), 0, decoded));
    CHECK(!coopnet::decodePlayerState(wire.data(), wire.size() - 1, decoded));

    std::vector<std::uint8_t> trailing = wire;
    trailing.push_back(0);
    CHECK(!coopnet::decodePlayerState(trailing.data(), trailing.size(), decoded));

    std::vector<std::uint8_t> wrongType = wire;
    wrongType[0] = 0x10; // a chat opcode is not a player state
    CHECK(!coopnet::decodePlayerState(wrongType.data(), wrongType.size(), decoded));

    // NaN must not survive the wire.
    std::vector<std::uint8_t> nanWire = wire;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    std::uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(nan));
    for (std::size_t i = 0; i < sizeof(bits); ++i) {
        auto* source = reinterpret_cast<const std::uint8_t*>(&nan);
        bits |= static_cast<std::uint32_t>(source[i]) << (8 * i);
    }
    for (std::size_t i = 0; i < sizeof(bits); ++i) {
        nanWire[5 + i] = static_cast<std::uint8_t>((bits >> (8 * i)) & 0xFF);
    }
    CHECK(!coopnet::decodePlayerState(nanWire.data(), nanWire.size(), decoded));
}

COOP_TEST(player_state_carries_body_state_last) {
    const coopnet::PlayerState original = sample();
    std::vector<std::uint8_t> wire;
    CHECK(coopnet::encodePlayerState(original, wire));
    CHECK_EQ(wire[25], std::uint8_t{0x34});
    CHECK_EQ(wire[26], std::uint8_t{0x12});
}

static_assert(coopnet::kPlayerStateType > 5,
              "player state opcode must stay outside the handshake range");
static_assert(coopnet::kPlayerStateType !=
                  static_cast<std::uint8_t>(0x10),
              "player state must not reuse the chat opcode");

COOP_TEST(player_gone_roundtrip) {
    std::vector<std::uint8_t> wire;
    CHECK(coopnet::encodePlayerGone(1002, wire));
    CHECK_EQ(wire.size(), coopnet::kPlayerGoneSize);

    std::uint32_t netId = 0;
    CHECK(coopnet::decodePlayerGone(wire.data(), wire.size(), netId));
    CHECK_EQ(netId, std::uint32_t{1002});
}

COOP_TEST(player_gone_rejects_malformed_input) {
    std::vector<std::uint8_t> wire;
    CHECK(!coopnet::encodePlayerGone(0, wire));

    CHECK(coopnet::encodePlayerGone(7, wire));
    std::uint32_t netId = 0;
    CHECK(!coopnet::decodePlayerGone(nullptr, wire.size(), netId));
    CHECK(!coopnet::decodePlayerGone(wire.data(), 0, netId));
    CHECK(!coopnet::decodePlayerGone(wire.data(), wire.size() - 1, netId));

    std::vector<std::uint8_t> trailing = wire;
    trailing.push_back(0);
    CHECK(!coopnet::decodePlayerGone(trailing.data(), trailing.size(), netId));

    std::vector<std::uint8_t> wrongType = wire;
    wrongType[0] = coopnet::kPlayerStateType;
    CHECK(!coopnet::decodePlayerGone(wrongType.data(), wrongType.size(), netId));

    std::vector<std::uint8_t> zeroId = wire;
    zeroId[1] = zeroId[2] = zeroId[3] = zeroId[4] = 0;
    CHECK(!coopnet::decodePlayerGone(zeroId.data(), zeroId.size(), netId));
}