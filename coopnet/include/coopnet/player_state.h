#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace coopnet {

// Transform of one player as seen by the authority. Sent on the control
// channel at a low rate; snapshots stay responsible for everything else.
struct PlayerState {
    std::uint32_t netId = 0;
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;
    float yaw = 0.f;
};

constexpr std::uint8_t kPlayerStateType = 0x11;
constexpr std::size_t kPlayerStateSize = 1 + 4 + 4 * 4;

// Wire format: [u8 type = 0x11][u32 net_id][f32 x][f32 y][f32 z][f32 yaw].
// Encoding fails for net_id 0 or non-finite coordinates.
bool encodePlayerState(const PlayerState& state, std::vector<std::uint8_t>& out);

// Strict decoding: wrong type, wrong size, non-finite values are errors.
bool decodePlayerState(const std::uint8_t* data, std::size_t size,
                       PlayerState& out);

} // namespace coopnet