#include "coopnet/spawn.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include "test_framework.h"

namespace {

coopnet::Spawn sample() {
    coopnet::Spawn spawn;
    spawn.netId = 1001;
    spawn.data = {0x4D, 0x5F, 0x53, 0x50, 0x41, 0x57, 0x4E, 0x00, 0x61};
    return spawn;
}

} // namespace

COOP_TEST(spawn_roundtrip) {
    const coopnet::Spawn original = sample();
    std::vector<std::uint8_t> wire;
    CHECK(coopnet::encodeSpawn(original, wire));
    CHECK_EQ(wire.size(), 7 + original.data.size());
    CHECK_EQ(wire[0], coopnet::kSpawnType);

    coopnet::Spawn decoded;
    CHECK(coopnet::decodeSpawn(wire.data(), wire.size(), decoded));
    CHECK_EQ(decoded.netId, original.netId);
    CHECK(decoded.data == original.data);
}

COOP_TEST(spawn_supports_max_blob) {
    coopnet::Spawn original;
    original.netId = 1004;
    original.data.assign(coopnet::kMaxSpawnBytes, 0xAB);

    std::vector<std::uint8_t> wire;
    CHECK(coopnet::encodeSpawn(original, wire));
    CHECK_EQ(wire.size(), 7 + coopnet::kMaxSpawnBytes);

    coopnet::Spawn decoded;
    CHECK(coopnet::decodeSpawn(wire.data(), wire.size(), decoded));
    CHECK_EQ(decoded.netId, original.netId);
    CHECK(decoded.data == original.data);
}

COOP_TEST(spawn_rejects_bad_values_on_encode) {
    std::vector<std::uint8_t> wire;

    coopnet::Spawn noId = sample();
    noId.netId = 0;
    CHECK(!coopnet::encodeSpawn(noId, wire));

    coopnet::Spawn empty = sample();
    empty.data.clear();
    CHECK(!coopnet::encodeSpawn(empty, wire));

    coopnet::Spawn huge = sample();
    huge.data.assign(coopnet::kMaxSpawnBytes + 1, 0);
    CHECK(!coopnet::encodeSpawn(huge, wire));
}

COOP_TEST(spawn_rejects_malformed_input) {
    const coopnet::Spawn original = sample();
    std::vector<std::uint8_t> wire;
    CHECK(coopnet::encodeSpawn(original, wire));

    coopnet::Spawn decoded;
    CHECK(!coopnet::decodeSpawn(nullptr, wire.size(), decoded));
    CHECK(!coopnet::decodeSpawn(wire.data(), 0, decoded));
    CHECK(!coopnet::decodeSpawn(wire.data(), wire.size() - 1, decoded));

    std::vector<std::uint8_t> trailing = wire;
    trailing.push_back(0);
    CHECK(!coopnet::decodeSpawn(trailing.data(), trailing.size(), decoded));

    std::vector<std::uint8_t> wrongType = wire;
    wrongType[0] = coopnet::kSpawnGoneType;
    CHECK(!coopnet::decodeSpawn(wrongType.data(), wrongType.size(), decoded));

    std::vector<std::uint8_t> zeroId = wire;
    zeroId[1] = zeroId[2] = zeroId[3] = zeroId[4] = 0;
    CHECK(!coopnet::decodeSpawn(zeroId.data(), zeroId.size(), decoded));

    // A length field that disagrees with the payload must be rejected even if
    // the buffer would otherwise be long enough.
    std::vector<std::uint8_t> wrongLength = wire;
    wrongLength[5] = 0xFF;
    wrongLength[6] = 0x00;
    CHECK(!coopnet::decodeSpawn(wrongLength.data(), wrongLength.size(), decoded));

    std::vector<std::uint8_t> zeroLength = wire;
    zeroLength[5] = zeroLength[6] = 0;
    zeroLength.resize(7);
    CHECK(!coopnet::decodeSpawn(zeroLength.data(), zeroLength.size(), decoded));
}

static_assert(coopnet::kSpawnType > 5,
              "spawn opcode must stay outside the handshake range");
static_assert(coopnet::kSpawnType != coopnet::kSpawnGoneType,
              "spawn and despawn need distinct opcodes");
static_assert(coopnet::kSpawnType != static_cast<std::uint8_t>(0x10) &&
                  coopnet::kSpawnType != static_cast<std::uint8_t>(0x11) &&
                  coopnet::kSpawnType != static_cast<std::uint8_t>(0x12) &&
                  coopnet::kSpawnType != static_cast<std::uint8_t>(0x13),
              "spawn opcode must not reuse an existing application message");

COOP_TEST(spawn_gone_roundtrip) {
    std::vector<std::uint8_t> wire;
    CHECK(coopnet::encodeSpawnGone(1002, wire));
    CHECK_EQ(wire.size(), std::size_t{5});
    CHECK_EQ(wire[0], coopnet::kSpawnGoneType);

    std::uint32_t netId = 0;
    CHECK(coopnet::decodeSpawnGone(wire.data(), wire.size(), netId));
    CHECK_EQ(netId, std::uint32_t{1002});
}

COOP_TEST(spawn_gone_rejects_malformed_input) {
    std::vector<std::uint8_t> wire;
    CHECK(!coopnet::encodeSpawnGone(0, wire));

    CHECK(coopnet::encodeSpawnGone(7, wire));
    std::uint32_t netId = 0;
    CHECK(!coopnet::decodeSpawnGone(nullptr, wire.size(), netId));
    CHECK(!coopnet::decodeSpawnGone(wire.data(), 0, netId));
    CHECK(!coopnet::decodeSpawnGone(wire.data(), wire.size() - 1, netId));

    std::vector<std::uint8_t> trailing = wire;
    trailing.push_back(0);
    CHECK(!coopnet::decodeSpawnGone(trailing.data(), trailing.size(), netId));

    std::vector<std::uint8_t> wrongType = wire;
    wrongType[0] = coopnet::kSpawnType;
    CHECK(!coopnet::decodeSpawnGone(wrongType.data(), wrongType.size(), netId));

    std::vector<std::uint8_t> zeroId = wire;
    zeroId[1] = zeroId[2] = zeroId[3] = zeroId[4] = 0;
    CHECK(!coopnet::decodeSpawnGone(zeroId.data(), zeroId.size(), netId));
}