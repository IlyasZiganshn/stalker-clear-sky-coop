#include "coopnet/interp.h"

#include <cmath>
#include <map>

namespace coopnet {

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;

float lerp(float a, float b, float t) { return a + (b - a) * t; }

Vec3 lerp(const Vec3& a, const Vec3& b, float t) {
    return {lerp(a.x, b.x, t), lerp(a.y, b.y, t), lerp(a.z, b.z, t)};
}

float lerpYaw(float a, float b, float t) { return a + wrapAngle(b - a) * t; }

EntityState blend(const EntityState& older, const EntityState& newer, float t) {
    EntityState result = older;
    result.present = newer.present;
    result.classId = newer.classId;

    if ((older.present & kFieldPosition) != 0) {
        result.position = (newer.present & kFieldPosition) != 0
                              ? lerp(older.position, newer.position, t)
                              : older.position;
    } else if ((newer.present & kFieldPosition) != 0) {
        result.position = newer.position;
    }

    if ((older.present & kFieldVelocity) != 0) {
        result.velocity = (newer.present & kFieldVelocity) != 0
                              ? lerp(older.velocity, newer.velocity, t)
                              : older.velocity;
    } else if ((newer.present & kFieldVelocity) != 0) {
        result.velocity = newer.velocity;
    }

    if ((older.present & kFieldAngles) != 0) {
        result.yaw = (newer.present & kFieldAngles) != 0
                         ? lerpYaw(older.yaw, newer.yaw, t)
                         : older.yaw;
        result.pitch = (newer.present & kFieldAngles) != 0
                           ? lerp(older.pitch, newer.pitch, t)
                           : older.pitch;
    } else if ((newer.present & kFieldAngles) != 0) {
        result.yaw = newer.yaw;
        result.pitch = newer.pitch;
    }

    return result;
}

}  // namespace

float wrapAngle(float radians) {
    radians = std::fmod(radians + kPi, kTwoPi);
    if (radians < 0.0f) {
        radians += kTwoPi;
    }
    return radians - kPi;
}

void InterpolationBuffer::push(std::uint32_t tick, std::uint32_t timeMs,
                               std::vector<EntityState> entities) {
    if (!samples_.empty() && timeMs < samples_.back().timeMs) {
        return;
    }
    samples_.push_back({tick, timeMs, std::move(entities)});
    while (samples_.size() > kMaxSamples) {
        samples_.pop_front();
    }
}

void InterpolationBuffer::sample(std::uint32_t nowMs, std::uint32_t delayMs,
                                 std::vector<EntityState>& out) const {
    out.clear();
    if (samples_.empty()) {
        return;
    }

    const std::uint32_t renderTime = nowMs > delayMs ? nowMs - delayMs : 0;
    if (renderTime <= samples_.front().timeMs) {
        out = samples_.front().entities;
        return;
    }
    if (renderTime >= samples_.back().timeMs) {
        out = samples_.back().entities;
        return;
    }

    std::size_t next = 1;
    while (samples_[next].timeMs <= renderTime) {
        ++next;
    }
    const Sample& older = samples_[next - 1];
    const Sample& newer = samples_[next];
    const float t = static_cast<float>(renderTime - older.timeMs) /
                    static_cast<float>(newer.timeMs - older.timeMs);

    std::map<std::uint32_t, const EntityState*> previous;
    for (const auto& entity : older.entities) {
        previous.emplace(entity.netId, &entity);
    }

    // The newer sample defines which entities exist: a removal takes effect
    // once the render time passes the sample that removed it.
    out.reserve(newer.entities.size());
    for (const auto& entity : newer.entities) {
        auto it = previous.find(entity.netId);
        out.push_back(it == previous.end() ? entity : blend(*it->second, entity, t));
    }
}

}  // namespace coopnet