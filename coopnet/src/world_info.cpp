#include "coopnet/world_info.h"

#include <cstring>

namespace coopnet {

namespace {

void putU64(std::uint8_t* out, std::uint64_t value) {
    for (std::size_t i = 0; i < 8; ++i) {
        out[i] = static_cast<std::uint8_t>((value >> (8 * i)) & 0xFF);
    }
}

std::uint64_t getU64(const std::uint8_t* in) {
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < 8; ++i) {
        value |= static_cast<std::uint64_t>(in[i]) << (8 * i);
    }
    return value;
}

} // namespace

bool encodeWorldInfo(const WorldInfo& info, std::vector<std::uint8_t>& out) {
    if (info.levelName.empty() || info.levelName.size() > kMaxLevelNameBytes) {
        return false;
    }
    out.resize(1 + 8 + 1 + info.levelName.size());
    out[0] = kWorldInfoType;
    putU64(out.data() + 1, info.gameTimeMs);
    out[9] = static_cast<std::uint8_t>(info.levelName.size());
    std::memcpy(out.data() + 10, info.levelName.data(), info.levelName.size());
    return true;
}

bool decodeWorldInfo(const std::uint8_t* data, std::size_t size,
                     WorldInfo& out) {
    if (data == nullptr || size < 10 || data[0] != kWorldInfoType) {
        return false;
    }
    const std::size_t length = data[9];
    if (length == 0 || length > kMaxLevelNameBytes) {
        return false;
    }
    if (size != 10 + length) {
        return false;
    }
    WorldInfo info;
    info.gameTimeMs = getU64(data + 1);
    info.levelName.assign(reinterpret_cast<const char*>(data + 10), length);
    out = info;
    return true;
}

} // namespace coopnet