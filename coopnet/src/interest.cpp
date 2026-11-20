#include "coopnet/interest.h"

#include <utility>

namespace coopnet {

namespace {

float distanceSquared(const Vec3& a, const Vec3& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

} // namespace

InterestFilter::InterestFilter(InterestConfig config) : config_(config) {
    if (config_.keepRadius < config_.radius) {
        config_.keepRadius = config_.radius;
    }
}

Snapshot InterestFilter::filter(const Snapshot& world, const Vec3& viewer) {
    Snapshot out;
    out.tick = world.tick;
    out.baseTick = 0;

    std::set<std::uint32_t> next;
    for (const auto& entity : world.entities) {
        const bool hadPosition = (entity.present & kFieldPosition) != 0;
        if (hadPosition) {
            const bool wasVisible = visible_.count(entity.netId) != 0;
            const float limit =
                wasVisible ? config_.keepRadius : config_.radius;
            if (distanceSquared(entity.position, viewer) > limit * limit) {
                continue;
            }
        }
        out.entities.push_back(entity);
        next.insert(entity.netId);
    }

    visible_ = std::move(next);
    return out;
}

} // namespace coopnet