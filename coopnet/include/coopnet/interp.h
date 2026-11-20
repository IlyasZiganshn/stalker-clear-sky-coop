#pragma once

#include <cstdint>
#include <deque>
#include <vector>

#include "coopnet/snapshot.h"

namespace coopnet {

// Client-side view of remote entities: snapshots are buffered by local arrival
// time and sampled at `now - delay`, so that motion stays smooth even though
// datagrams arrive in bursts. Entities must be complete (merged by
// SnapshotStore), not raw deltas.
class InterpolationBuffer {
public:
    static constexpr std::size_t kMaxSamples = 32;

    // Ignored if it is older than the newest buffered sample.
    void push(std::uint32_t tick, std::uint32_t timeMs,
              std::vector<EntityState> entities);

    // Entity states at `nowMs - delayMs`. Empty before the first sample; holds
    // the newest sample once the render time passes it (no extrapolation).
    void sample(std::uint32_t nowMs, std::uint32_t delayMs,
                std::vector<EntityState>& out) const;

    std::size_t size() const { return samples_.size(); }
    void clear() { samples_.clear(); }

private:
    struct Sample {
        std::uint32_t tick = 0;
        std::uint32_t timeMs = 0;
        std::vector<EntityState> entities;
    };

    std::deque<Sample> samples_;
};

// Shortest-arc difference between two yaw angles, in (-pi, pi].
float wrapAngle(float radians);

}  // namespace coopnet