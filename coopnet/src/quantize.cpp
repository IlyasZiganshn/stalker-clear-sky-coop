#include "coopnet/quantize.h"

#include <algorithm>
#include <cmath>

namespace coopnet {
namespace {

constexpr float kTwoPi = 6.283185307179586f;

float clampUnit(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

float encodeComponent(float value, float min, float extent, std::uint32_t levels) {
    if (extent <= 0.0f) {
        return 0.0f;
    }
    return std::round(clampUnit((value - min) / extent) * static_cast<float>(levels));
}

}  // namespace

PositionCodec::PositionCodec(const Vec3& min, const Vec3& max, int bits)
    : min_(min), max_(max), bits_(std::clamp(bits, 1, 31)) {
    levels_ = (1u << bits_) - 1u;
}

void PositionCodec::encode(const Vec3& p, std::uint32_t& qx, std::uint32_t& qy,
                           std::uint32_t& qz) const {
    qx = static_cast<std::uint32_t>(
        encodeComponent(p.x, min_.x, max_.x - min_.x, levels_));
    qy = static_cast<std::uint32_t>(
        encodeComponent(p.y, min_.y, max_.y - min_.y, levels_));
    qz = static_cast<std::uint32_t>(
        encodeComponent(p.z, min_.z, max_.z - min_.z, levels_));
}

Vec3 PositionCodec::decode(std::uint32_t qx, std::uint32_t qy,
                           std::uint32_t qz) const {
    const float scale = 1.0f / static_cast<float>(levels_);
    Vec3 p;
    p.x = min_.x + static_cast<float>(qx) * scale * (max_.x - min_.x);
    p.y = min_.y + static_cast<float>(qy) * scale * (max_.y - min_.y);
    p.z = min_.z + static_cast<float>(qz) * scale * (max_.z - min_.z);
    return p;
}

float PositionCodec::resolution() const {
    const float scale = 1.0f / static_cast<float>(levels_);
    return std::max({(max_.x - min_.x) * scale, (max_.y - min_.y) * scale,
                     (max_.z - min_.z) * scale});
}

std::uint16_t encodeAngle(float radians) {
    float wrapped = std::fmod(radians, kTwoPi);
    if (wrapped < 0.0f) {
        wrapped += kTwoPi;
    }
    const auto value = static_cast<std::uint32_t>(
        std::lround(wrapped / kTwoPi * 65535.0f));
    return static_cast<std::uint16_t>(value & 0xFFFFu);
}

float decodeAngle(std::uint16_t value) {
    return static_cast<float>(value) / 65535.0f * kTwoPi;
}

std::uint16_t encodePitch(float radians) {
    constexpr float kHalfPi = kTwoPi * 0.25f;
    const float clamped = std::clamp(radians, -kHalfPi, kHalfPi);
    const float normalized = (clamped + kHalfPi) / (2.0f * kHalfPi);
    const auto value = static_cast<std::uint32_t>(
        std::lround(normalized * 65535.0f));
    return static_cast<std::uint16_t>(std::min(value, 65535u));
}

float decodePitch(std::uint16_t value) {
    constexpr float kHalfPi = kTwoPi * 0.25f;
    return static_cast<float>(value) / 65535.0f * (2.0f * kHalfPi) - kHalfPi;
}

std::uint16_t encodeVelocity(float metersPerSecond, float range) {
    if (range <= 0.0f) {
        return 0u;
    }
    const float normalized = clampUnit((metersPerSecond + range) / (2.0f * range));
    const auto value = static_cast<std::uint32_t>(std::lround(normalized * 4095.0f));
    return static_cast<std::uint16_t>(std::min(value, 4095u));
}

float decodeVelocity(std::uint16_t value, float range) {
    return (static_cast<float>(value) / 4095.0f) * 2.0f * range - range;
}

std::uint32_t encodeUnit(float value, int bits) {
    const int clampedBits = std::clamp(bits, 1, 31);
    const std::uint32_t levels = (1u << clampedBits) - 1u;
    return static_cast<std::uint32_t>(
        std::lround(clampUnit(value) * static_cast<float>(levels)));
}

float decodeUnit(std::uint32_t value, int bits) {
    const int clampedBits = std::clamp(bits, 1, 31);
    const std::uint32_t levels = (1u << clampedBits) - 1u;
    return static_cast<float>(std::min(value, levels)) /
           static_cast<float>(levels);
}

}  // namespace coopnet