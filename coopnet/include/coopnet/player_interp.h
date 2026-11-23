#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "coopnet/player_state.h"

namespace coopnet {

// Smooths the low-rate player transform stream: keeps a history buffer of recent
// samples for every peer and samples smoothly in the past, so a remote doll moves
// continuously between updates instead of snapping.
class PlayerInterpolator {
public:
    static constexpr std::size_t kMaxTracks = 16;
    static constexpr std::size_t kMaxSamples = 16;

    // Out-of-order samples (older than the newest one) are ignored.
    void push(const PlayerState& state, std::uint32_t timeMs);
    void forget(std::uint32_t netId);
    void clear() { tracks_.clear(); }

    // Writes the transform of every peer seen within `maxAgeMs`, rendered at
    // `nowMs - delayMs`. Holds the newest sample once the render time passes
    // it (no extrapolation). Returns the number of states written.
    std::uint32_t sample(std::uint32_t nowMs, std::uint32_t delayMs,
                         std::uint32_t maxAgeMs, PlayerState* out,
                         std::uint32_t capacity) const;

    std::size_t size() const { return tracks_.size(); }

private:
    struct Sample {
        PlayerState state;
        std::uint32_t timeMs = 0;
    };

    struct Track {
        std::uint32_t netId = 0;
        std::vector<Sample> samples;
    };

    std::vector<Track> tracks_;
};

} // namespace coopnet
