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
        if (candidate.netId == state.netId) {
            track = &candidate;
            break;
        }
    }

    if (track == nullptr) {
        if (tracks_.size() >= kMaxTracks) {
            // Drop the peer that has been silent the longest
            std::size_t oldest = 0;
            for (std::size_t i = 1; i < tracks_.size(); ++i) {
                const std::uint32_t t_i = tracks_[i].samples.empty() ? 0 : tracks_[i].samples.back().timeMs;
                const std::uint32_t t_old = tracks_[oldest].samples.empty() ? 0 : tracks_[oldest].samples.back().timeMs;
                if (since(t_i, t_old) < 0) {
                    oldest = i;
                }
            }
            tracks_[oldest] = Track{};
            track = &tracks_[oldest];
        } else {
            tracks_.emplace_back();
            track = &tracks_.back();
        }
        track->netId = state.netId;
    }

    if (!track->samples.empty() && since(timeMs, track->samples.back().timeMs) <= 0) {
        return;
    }

    track->samples.push_back({state, timeMs});
    while (track->samples.size() > kMaxSamples) {
        track->samples.erase(track->samples.begin());
    }
}

void PlayerInterpolator::forget(std::uint32_t netId) {
    for (auto it = tracks_.begin(); it != tracks_.end(); ++it) {
        if (it->netId == netId) {
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

    const std::uint32_t renderMs = nowMs > delayMs ? nowMs - delayMs : 0;
    std::uint32_t written = 0;
    for (const auto& track : tracks_) {
        if (written >= capacity) {
            break;
        }
        if (track.samples.empty()) {
            continue;
        }
        const Sample& newest = track.samples.back();
        if (since(nowMs, newest.timeMs) > static_cast<std::int32_t>(maxAgeMs)) {
            continue;
        }

        PlayerState state = newest.state;
        if (track.samples.size() == 1) {
            out[written++] = newest.state;
            continue;
        }

        const Sample& oldest = track.samples.front();
        if (renderMs <= oldest.timeMs) {
            out[written++] = oldest.state;
            continue;
        }
        if (renderMs >= newest.timeMs) {
            out[written++] = newest.state;
            continue;
        }

        // Bracket renderMs between samples[i - 1] and samples[i]
        std::size_t next = 1;
        while (next < track.samples.size() && track.samples[next].timeMs <= renderMs) {
            ++next;
        }
        if (next >= track.samples.size()) {
            out[written++] = newest.state;
            continue;
        }

        const Sample& older = track.samples[next - 1];
        const Sample& newer = track.samples[next];
        const std::int32_t span = since(newer.timeMs, older.timeMs);
        const std::int32_t ago = since(renderMs, older.timeMs);
        if (span > 0 && ago >= 0) {
            const float t = static_cast<float>(ago) / static_cast<float>(span);
            state.x = lerp(older.state.x, newer.state.x, t);
            state.y = lerp(older.state.y, newer.state.y, t);
            state.z = lerp(older.state.z, newer.state.z, t);
            const float deltaYaw = wrapAngle(newer.state.yaw - older.state.yaw);
            state.yaw = wrapAngle(older.state.yaw + deltaYaw * t);
            // Discrete body state: kept from the older sample until render time reaches newer
            state.bodyState = older.state.bodyState;
        }
        out[written++] = state;
    }
    return written;
}

} // namespace coopnet
