#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace coopnet {

// Serialized spawn of one remote player. The blob is opaque to coopnet: the
// engine writes and reads its own CSE_Abstract bytes, coopnet only carries them
// and associates them with a net_id. A blob is sent once per player, reliably.
struct Spawn {
    std::uint32_t netId = 0;
    std::vector<std::uint8_t> data;
};

constexpr std::uint8_t kSpawnType = 0x14;
constexpr std::uint8_t kSpawnGoneType = 0x15;
constexpr std::size_t kMaxSpawnBytes = 4096;

// Wire format: [u8 type = 0x14][u32 net_id][u16 length][u8 data[length]].
// Encoding fails for net_id 0 and for an empty or oversized blob.
bool encodeSpawn(const Spawn& spawn, std::vector<std::uint8_t>& out);

// Strict decoding: wrong type, wrong size, zero net_id, empty blob are errors.
bool decodeSpawn(const std::uint8_t* data, std::size_t size, Spawn& out);

// The authority reports that a spawned player is gone for good; peers drop the
// entity instead of waiting for its transform to go stale.
// Wire format: [u8 type = 0x15][u32 net_id].
bool encodeSpawnGone(std::uint32_t netId, std::vector<std::uint8_t>& out);
bool decodeSpawnGone(const std::uint8_t* data, std::size_t size,
                     std::uint32_t& netId);

} // namespace coopnet