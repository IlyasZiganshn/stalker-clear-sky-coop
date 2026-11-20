#include "coopnet/quantize.h"

#include <cmath>

#include "coopnet/bitstream.h"
#include "test_framework.h"

using namespace coopnet;

namespace {

// Deterministic generator: no dependence on <random> implementation details.
struct Lcg {
    std::uint32_t state = 0x12345678u;

    std::uint32_t next() {
        state = state * 1664525u + 1013904223u;
        return state;
    }

    float unit() { return static_cast<float>(next() >> 8) / 16777216.0f; }
};

}  // namespace

COOP_TEST(quantize_position_resolution_for_typical_level) {
    const PositionCodec codec({-100.0f, -50.0f, 0.0f}, {100.0f, 50.0f, 20.0f}, 15);
    CHECK_EQ(codec.bitsPerComponent(), 15);
    // 200 m over 32767 steps: under 1 cm.
    CHECK_NEAR(codec.resolution(), 200.0f / 32767.0f, 1e-4f);
    CHECK(codec.resolution() < 0.01f);
}

COOP_TEST(quantize_position_roundtrip_stays_within_resolution) {
    const PositionCodec codec({-100.0f, -50.0f, 0.0f}, {100.0f, 50.0f, 20.0f}, 15);
    const float tol = codec.resolution() + 1e-4f;
    Lcg rng;

    for (int i = 0; i < 2000; ++i) {
        Vec3 original;
        original.x = -100.0f + rng.unit() * 200.0f;
        original.y = -50.0f + rng.unit() * 100.0f;
        original.z = rng.unit() * 20.0f;

        std::uint32_t qx = 0, qy = 0, qz = 0;
        codec.encode(original, qx, qy, qz);
        CHECK(qx <= 32767u);
        CHECK(qy <= 32767u);
        CHECK(qz <= 32767u);

        const Vec3 decoded = codec.decode(qx, qy, qz);
        CHECK_NEAR(decoded.x, original.x, tol);
        CHECK_NEAR(decoded.y, original.y, tol);
        CHECK_NEAR(decoded.z, original.z, tol);
    }
}

COOP_TEST(quantize_position_clamps_outside_bounds) {
    const PositionCodec codec({-10.0f, -10.0f, -10.0f}, {10.0f, 10.0f, 10.0f}, 15);
    std::uint32_t qx = 0, qy = 0, qz = 0;

    codec.encode({-1000.0f, 1000.0f, 0.0f}, qx, qy, qz);
    CHECK_EQ(qx, 0u);
    CHECK_EQ(qy, 32767u);
    // Midpoint of an odd number of steps is not representable exactly.
    CHECK_NEAR(codec.decode(qx, qy, qz).z, 0.0f, 0.5f * codec.resolution() + 1e-4f);
}

COOP_TEST(quantize_position_degenerate_bounds_do_not_nan) {
    const PositionCodec codec({1.0f, 2.0f, 3.0f}, {1.0f, 2.0f, 3.0f}, 15);
    std::uint32_t qx = 0, qy = 0, qz = 0;
    codec.encode({5.0f, 5.0f, 5.0f}, qx, qy, qz);
    CHECK_EQ(qx, 0u);
    const Vec3 decoded = codec.decode(qx, qy, qz);
    CHECK(decoded.x == 1.0f);
    CHECK(std::isfinite(codec.resolution()));
}

COOP_TEST(quantize_angle_roundtrip) {
    const float values[] = {0.0f, 0.5f, 1.0f, 3.0f, -1.0f, -3.0f, 6.0f};
    for (const float value : values) {
        const std::uint16_t encoded = encodeAngle(value);
        float decoded = decodeAngle(encoded);
        // Compare on the circle: 0 and 2*pi are the same direction.
        const float two_pi = 6.283185307179586f;
        float diff = std::fabs(decoded - value);
        if (diff > two_pi * 0.5f) {
            diff = two_pi - diff;
        }
        CHECK(diff < 0.0005f);
    }
}

COOP_TEST(quantize_pitch_is_signed_and_clamped) {
    CHECK_NEAR(decodePitch(encodePitch(0.0f)), 0.0f, 0.0001f);
    CHECK_NEAR(decodePitch(encodePitch(1.0f)), 1.0f, 0.0002f);
    CHECK_NEAR(decodePitch(encodePitch(-1.0f)), -1.0f, 0.0002f);
    // Looking straight down must stay down, not wrap to 2*pi.
    CHECK_NEAR(decodePitch(encodePitch(-1.5707f)), -1.5707f, 0.0005f);
    CHECK_NEAR(decodePitch(encodePitch(-10.0f)), -1.5708f, 0.0005f);
    CHECK_NEAR(decodePitch(encodePitch(10.0f)), 1.5708f, 0.0005f);
}

COOP_TEST(quantize_velocity_roundtrip_and_clamp) {
    CHECK_NEAR(decodeVelocity(encodeVelocity(0.0f)), 0.0f, 0.01f);
    CHECK_NEAR(decodeVelocity(encodeVelocity(10.0f)), 10.0f, 0.05f);
    CHECK_NEAR(decodeVelocity(encodeVelocity(-10.0f)), -10.0f, 0.05f);
    CHECK_NEAR(decodeVelocity(encodeVelocity(1000.0f)), 32.0f, 0.01f);
    CHECK_NEAR(decodeVelocity(encodeVelocity(-1000.0f)), -32.0f, 0.01f);
    CHECK(encodeVelocity(0.0f) <= 4095u);
}

COOP_TEST(quantize_unit_range) {
    CHECK_EQ(encodeUnit(0.0f, 8), 0u);
    CHECK_EQ(encodeUnit(1.0f, 8), 255u);
    CHECK_EQ(encodeUnit(-5.0f, 8), 0u);
    CHECK_EQ(encodeUnit(5.0f, 8), 255u);
    CHECK_NEAR(decodeUnit(128u, 8), 128.0f / 255.0f, 1e-6f);
    CHECK_NEAR(decodeUnit(999u, 8), 1.0f, 1e-6f);
}

COOP_TEST(quantize_entity_record_on_the_wire) {
    // A minimal snapshot record: net id, flags, position, angles, health.
    const PositionCodec codec({-100.0f, -50.0f, 0.0f}, {100.0f, 50.0f, 20.0f}, 15);

    BitWriter writer;
    std::uint32_t qx = 0, qy = 0, qz = 0;
    const Vec3 position{12.5f, -3.25f, 1.75f};
    codec.encode(position, qx, qy, qz);

    writer.writeU32(4242u);
    writer.writeU8(0x03u);
    writer.writeU16(static_cast<std::uint16_t>(qx));
    writer.writeU16(static_cast<std::uint16_t>(qy));
    writer.writeU16(static_cast<std::uint16_t>(qz));
    writer.writeU16(encodeAngle(1.0f));
    writer.writeU16(encodePitch(-0.2f));
    writer.writeU16(100u);

    CHECK_EQ(writer.bitSize(), std::size_t{32 + 8 + 3 * 16 + 2 * 16 + 16});

    BitReader reader(writer.data().data(), writer.data().size());
    CHECK_EQ(reader.readU32(), 4242u);
    CHECK_EQ(reader.readU8(), 0x03u);
    const auto rx = static_cast<std::uint32_t>(reader.readU16());
    const auto ry = static_cast<std::uint32_t>(reader.readU16());
    const auto rz = static_cast<std::uint32_t>(reader.readU16());
    const float yaw = decodeAngle(reader.readU16());
    const float pitch = decodePitch(reader.readU16());
    CHECK_EQ(reader.readU16(), 100u);
    CHECK(reader.ok());

    const Vec3 decoded = codec.decode(rx, ry, rz);
    const float tol = codec.resolution() + 1e-4f;
    CHECK_NEAR(decoded.x, position.x, tol);
    CHECK_NEAR(decoded.y, position.y, tol);
    CHECK_NEAR(decoded.z, position.z, tol);
    CHECK_NEAR(yaw, 1.0f, 0.0005f);
    CHECK_NEAR(pitch, -0.2f, 0.0005f);
}