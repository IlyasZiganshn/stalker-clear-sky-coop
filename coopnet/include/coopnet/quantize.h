#pragma once

#include <cstdint>

namespace coopnet {

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

// Position inside the level AABB, 15 bits per component by default. For a
// 500 m level that is ~1.5 cm; for a 200 m level, ~6 mm. Points outside the
// bounds are clamped, so bounds must come from the level itself.
class PositionCodec {
public:
    PositionCodec() = default;
    PositionCodec(const Vec3& min, const Vec3& max, int bits = 15);

    void encode(const Vec3& p, std::uint32_t& qx, std::uint32_t& qy,
                std::uint32_t& qz) const;
    Vec3 decode(std::uint32_t qx, std::uint32_t qy, std::uint32_t qz) const;

    int bitsPerComponent() const { return bits_; }
    // Worst-case error per axis in meters.
    float resolution() const;

private:
    Vec3 min_{};
    Vec3 max_{};
    int bits_ = 15;
    std::uint32_t levels_ = 32767;
};

// Yaw: full turn over 16 bits (~0.0055 degrees). Decoded value is always in
// [0, 2*pi); compare directions on the circle, not as plain floats.
std::uint16_t encodeAngle(float radians);
float decodeAngle(std::uint16_t value);

// Pitch: [-pi/2, pi/2] over 16 bits, no wrapping. Values outside are clamped.
std::uint16_t encodePitch(float radians);
float decodePitch(std::uint16_t value);

// Signed velocity component in [-range, range] m/s over 12 bits.
std::uint16_t encodeVelocity(float metersPerSecond, float range = 32.0f);
float decodeVelocity(std::uint16_t value, float range = 32.0f);

// Generic normalized value in [0, 1] over `bits` bits.
std::uint32_t encodeUnit(float value, int bits);
float decodeUnit(std::uint32_t value, int bits);

}  // namespace coopnet