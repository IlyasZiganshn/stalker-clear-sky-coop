#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "coopnet/player_state.h"

namespace coopnet {

// Smooths the low-rate player transform stream: the last two samples of every
// peer are kept and sampled slightly in the past, so a remote doll moves
// continuously between 20 Hz updates instead of snapping. Discrete fields
// (body state) are taken from the older sample.
class PlayerInterpolator {
public:
    static constexpr std::size_t kMaxTracks = 16;

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
    struct Track {
        PlayerState previous;
        PlayerState latest;
        std::uint32_t previousMs = 0;
        std::uint32_t latestMs = 0;
        bool hasPrevious = false;
    };

    std::vector<Track> tracks_;
};

} // namespace coopnet