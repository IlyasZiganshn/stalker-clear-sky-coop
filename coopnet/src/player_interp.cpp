#include "coopnet/player_interp.h"

#include <cmath>

#include "coopnet/interp.h"

namespace coopnet {
namespace {

float lerp(float from, float to, float t) { return from + (to - from) * t; }

std::int32_t since(std::uint32_t timeMs, std::uint32_t referenceMs) {
    return static_cast<std::int32_t>(timeMs - referenceMs);
}

} // namespace

void PlayerInterpolator::push(const PlayerState& state, std::uint32_t timeMs) {
    if (state.netId == 0) {
        return;
    }

    Track* track = nullptr;
    for (auto& candidate : tracks_) {
        if (candidate.latest.netId == state.netId) {
            track = &candidate;
            break;
        }
    }

    if (track == nullptr) {
        if (tracks_.size() >= kMaxTracks) {
            // Drop the peer that has been silent the longest; a real session
            // never gets here because leave/despawn forget tracks.
            std::size_t oldest = 0;
            for (std::size_t i = 1; i < tracks_.size(); ++i) {
                if (since(tracks_[i].latestMs, tracks_[oldest].latestMs) < 0) {
                    oldest = i;
                }
            }
            tracks_[oldest] = Track{};
            track = &tracks_[oldest];
        } else {
            tracks_.emplace_back();
            track = &tracks_.back();
        }
        track->latest = state;
        track->latestMs = timeMs;
        return;
    }

    if (since(timeMs, track->latestMs) <= 0) {
        return;
    }

    track->previous = track->latest;
    track->previousMs = track->latestMs;
    track->hasPrevious = true;
    track->latest = state;
    track->latestMs = timeMs;
}

void PlayerInterpolator::forget(std::uint32_t netId) {
    for (auto it = tracks_.begin(); it != tracks_.end(); ++it) {
        if (it->latest.netId == netId) {
            tracks_.erase(it);
            return;
        }
    }
}

std::uint32_t PlayerInterpolator::sample(std::uint32_t nowMs,
                                         std::uint32_t delayMs,
                                         std::uint32_t maxAgeMs,
                                         PlayerState* out,
                                         std::uint32_t capacity) const {
    if (out == nullptr || capacity == 0) {
        return 0;
    }

    const std::uint32_t renderMs = nowMs - delayMs;
    std::uint32_t written = 0;
    for (const auto& track : tracks_) {
        if (written >= capacity) {
            break;
        }
        if (since(nowMs, track.latestMs) > static_cast<std::int32_t>(maxAgeMs)) {
            continue;
        }

        PlayerState state = track.latest;
        if (track.hasPrevious) {
            const std::int32_t span =
                since(track.latestMs, track.previousMs);
            const std::int32_t ago = since(renderMs, track.previousMs);
            if (span > 0) {
                if (ago <= 0) {
                    state = track.previous;
                } else if (ago < span) {
                    const float t = static_cast<float>(ago) /
                                    static_cast<float>(span);
                    state.x = lerp(track.previous.x, track.latest.x, t);
                    state.y = lerp(track.previous.y, track.latest.y, t);
                    state.z = lerp(track.previous.z, track.latest.z, t);
                    const float deltaYaw =
                        wrapAngle(track.latest.yaw - track.previous.yaw);
                    state.yaw =
                        wrapAngle(track.previous.yaw + deltaYaw * t);
                    // Body state is discrete: blend it would be meaningless, so
                    // it switches together with the sample it belongs to.
                    state.bodyState = track.previous.bodyState;
                }
            }
        }
        out[written++] = state;
    }
    return written;
}

} // namespace coopnet