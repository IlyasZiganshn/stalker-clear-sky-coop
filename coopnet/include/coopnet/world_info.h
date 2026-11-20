#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace coopnet {

// What a session is attached to: the level and the game clock. The authority
// owns both; clients compare their level name and offset their clock.
struct WorldInfo {
    std::uint64_t gameTimeMs = 0;
    std::string levelName;
};

constexpr std::uint8_t kWorldInfoType = 0x13;
constexpr std::size_t kMaxLevelNameBytes = 64;

// Wire format: [u8 type = 0x13][u64 game_time_ms][u8 length][u8 name[length]].
// Encoding fails for an empty or too long level name.
bool encodeWorldInfo(const WorldInfo& info, std::vector<std::uint8_t>& out);

// Strict decoding: wrong type, wrong size, empty or oversized name are errors.
bool decodeWorldInfo(const std::uint8_t* data, std::size_t size, WorldInfo& out);

} // namespace coopnet