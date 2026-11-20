#include "coopnet/spawn.h"

#include <cstring>
#include <utility>

namespace coopnet {

namespace {

void putU16(std::uint8_t* out, std::uint16_t value) {
    out[0] = static_cast<std::uint8_t>(value & 0xFF);
    out[1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
}

std::uint16_t getU16(const std::uint8_t* in) {
    return static_cast<std::uint16_t>(in[0]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(in[1]) << 8);
}

void putU32(std::uint8_t* out, std::uint32_t value) {
    out[0] = static_cast<std::uint8_t>(value & 0xFF);
    out[1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
    out[2] = static_cast<std::uint8_t>((value >> 16) & 0xFF);
    out[3] = static_cast<std::uint8_t>((value >> 24) & 0xFF);
}

std::uint32_t getU32(const std::uint8_t* in) {
    return static_cast<std::uint32_t>(in[0]) |
           (static_cast<std::uint32_t>(in[1]) << 8) |
           (static_cast<std::uint32_t>(in[2]) << 16) |
           (static_cast<std::uint32_t>(in[3]) << 24);
}

} // namespace

bool encodeSpawn(const Spawn& spawn, std::vector<std::uint8_t>& out) {
    if (spawn.netId == 0 || spawn.data.empty() ||
        spawn.data.size() > kMaxSpawnBytes) {
        return false;
    }
    out.resize(1 + 4 + 2 + spawn.data.size());
    out[0] = kSpawnType;
    putU32(out.data() + 1, spawn.netId);
    putU16(out.data() + 5, static_cast<std::uint16_t>(spawn.data.size()));
    std::memcpy(out.data() + 7, spawn.data.data(), spawn.data.size());
    return true;
}

bool decodeSpawn(const std::uint8_t* data, std::size_t size, Spawn& out) {
    if (data == nullptr || size < 7 || data[0] != kSpawnType) {
        return false;
    }
    const std::uint32_t netId = getU32(data + 1);
    const std::size_t length = getU16(data + 5);
    if (netId == 0 || length == 0 || length > kMaxSpawnBytes) {
        return false;
    }
    if (size != 7 + length) {
        return false;
    }
    Spawn spawn;
    spawn.netId = netId;
    spawn.data.assign(data + 7, data + 7 + length);
    out = std::move(spawn);
    return true;
}

bool encodeSpawnGone(std::uint32_t netId, std::vector<std::uint8_t>& out) {
    if (netId == 0) {
        return false;
    }
    out.resize(1 + 4);
    out[0] = kSpawnGoneType;
    putU32(out.data() + 1, netId);
    return true;
}

bool decodeSpawnGone(const std::uint8_t* data, std::size_t size,
                     std::uint32_t& netId) {
    if (data == nullptr || size != 1 + 4 || data[0] != kSpawnGoneType) {
        return false;
    }
    const std::uint32_t decoded = getU32(data + 1);
    if (decoded == 0) {
        return false;
    }
    netId = decoded;
    return true;
}

} // namespace coopnet