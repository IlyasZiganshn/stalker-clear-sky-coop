#pragma once

#include <cstddef>
#include <cstdint>
#include <set>

#include "coopnet/snapshot.h"

namespace coopnet {

struct InterestConfig {
    float radius = 150.0f;     // entities farther than this are not sent
    float keepRadius = 165.0f; // hysteresis: keep what was already sent
};

// Per-client filter: trims a full world snapshot to the entities around the
// viewer. Stateful: an entity already visible is kept until it leaves
// keepRadius, which prevents flapping at the boundary. Entities without a
// position field cannot be measured and are always kept.
class InterestFilter {
public:
    explicit InterestFilter(InterestConfig config = {});

    // `world` must be a full state (baseTick == 0). The viewer is that client's
    // position. The result is a full state for SnapshotSender::next.
    Snapshot filter(const Snapshot& world, const Vec3& viewer);

    void reset() { visible_.clear(); }
    std::size_t visibleCount() const { return visible_.size(); }

private:
    InterestConfig config_;
    std::set<std::uint32_t> visible_;
};

} // namespace coopnet