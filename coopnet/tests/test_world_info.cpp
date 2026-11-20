#include "coopnet/world_info.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "test_framework.h"

namespace {

coopnet::WorldInfo sample() {
    coopnet::WorldInfo info;
    info.gameTimeMs = 0x0123456789ABCDEFull;
    info.levelName = "l01_escape";
    return info;
}

} // namespace

COOP_TEST(world_info_roundtrip) {
    const coopnet::WorldInfo original = sample();
    std::vector<std::uint8_t> wire;
    CHECK(coopnet::encodeWorldInfo(original, wire));
    CHECK_EQ(wire.size(), std::size_t{10 + original.levelName.size()});

    coopnet::WorldInfo decoded;
    CHECK(coopnet::decodeWorldInfo(wire.data(), wire.size(), decoded));
    CHECK_EQ(decoded.gameTimeMs, original.gameTimeMs);
    CHECK(decoded.levelName == original.levelName);
}

COOP_TEST(world_info_rejects_bad_names_on_encode) {
    std::vector<std::uint8_t> wire;

    coopnet::WorldInfo empty = sample();
    empty.levelName.clear();
    CHECK(!coopnet::encodeWorldInfo(empty, wire));

    coopnet::WorldInfo oversized = sample();
    oversized.levelName.assign(coopnet::kMaxLevelNameBytes + 1, 'x');
    CHECK(!coopnet::encodeWorldInfo(oversized, wire));

    oversized.levelName.assign(coopnet::kMaxLevelNameBytes, 'x');
    CHECK(coopnet::encodeWorldInfo(oversized, wire));
}

COOP_TEST(world_info_rejects_malformed_input) {
    const coopnet::WorldInfo original = sample();
    std::vector<std::uint8_t> wire;
    CHECK(coopnet::encodeWorldInfo(original, wire));

    coopnet::WorldInfo decoded;
    CHECK(!coopnet::decodeWorldInfo(nullptr, wire.size(), decoded));
    CHECK(!coopnet::decodeWorldInfo(wire.data(), 0, decoded));
    CHECK(!coopnet::decodeWorldInfo(wire.data(), wire.size() - 1, decoded));

    std::vector<std::uint8_t> trailing = wire;
    trailing.push_back(0);
    CHECK(!coopnet::decodeWorldInfo(trailing.data(), trailing.size(), decoded));

    std::vector<std::uint8_t> wrongType = wire;
    wrongType[0] = 0x11;
    CHECK(!coopnet::decodeWorldInfo(wrongType.data(), wrongType.size(), decoded));

    std::vector<std::uint8_t> zeroLength = wire;
    zeroLength[9] = 0;
    CHECK(!coopnet::decodeWorldInfo(zeroLength.data(), zeroLength.size(), decoded));

    std::vector<std::uint8_t> absurdLength = wire;
    absurdLength[9] = 0xFF;
    CHECK(!coopnet::decodeWorldInfo(absurdLength.data(), absurdLength.size(),
                                    decoded));
}

static_assert(coopnet::kWorldInfoType > 5,
              "world info opcode must stay outside the handshake range");
static_assert(coopnet::kWorldInfoType != 0x10 &&
                  coopnet::kWorldInfoType != 0x11 &&
                  coopnet::kWorldInfoType != 0x12,
              "world info must not reuse chat or player opcodes");