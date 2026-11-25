#include "coopnet/player_state.h"

#include <cmath>
#include <cstring>

namespace coopnet {

namespace {

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

void putU16(std::uint8_t* out, std::uint16_t value) {
    out[0] = static_cast<std::uint8_t>(value & 0xFF);
    out[1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
}

std::uint16_t getU16(const std::uint8_t* in) {
    return static_cast<std::uint16_t>(static_cast<std::uint16_t>(in[0]) |
                                      (static_cast<std::uint16_t>(in[1]) << 8));
}

void putF32(std::uint8_t* out, float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    putU32(out, bits);
}

float getF32(const std::uint8_t* in) {
    const std::uint32_t bits = getU32(in);
    float value = 0.f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

} // namespace

bool encodePlayerState(const PlayerState& state,
                       std::vector<std::uint8_t>& out) {
    if (state.netId == 0) {
        return false;
    }
    if (!std::isfinite(state.x) || !std::isfinite(state.y) ||
        !std::isfinite(state.z) || !std::isfinite(state.yaw) ||
        !std::isfinite(state.pitch)) {
        return false;
    }
    out.resize(kPlayerStateSize);
    out[0] = kPlayerStateType;
    putU32(out.data() + 1, state.netId);
    putF32(out.data() + 5, state.x);
    putF32(out.data() + 9, state.y);
    putF32(out.data() + 13, state.z);
    putF32(out.data() + 17, state.yaw);
    putF32(out.data() + 21, state.pitch);
    putU16(out.data() + 25, state.bodyState);
    return true;
}

bool decodePlayerState(const std::uint8_t* data, std::size_t size,
                       PlayerState& out) {
    if (data == nullptr || size != kPlayerStateSize || data[0] != kPlayerStateType) {
        return false;
    }
    PlayerState state;
    state.netId = getU32(data + 1);
    state.x = getF32(data + 5);
    state.y = getF32(data + 9);
    state.z = getF32(data + 13);
    state.yaw = getF32(data + 17);
    state.pitch = getF32(data + 21);
    state.bodyState = getU16(data + 25);
    if (state.netId == 0 || !std::isfinite(state.x) ||
        !std::isfinite(state.y) || !std::isfinite(state.z) ||
        !std::isfinite(state.yaw) || !std::isfinite(state.pitch)) {
        return false;
    }
    out = state;
    return true;
}

bool encodePlayerGone(std::uint32_t netId, std::vector<std::uint8_t>& out) {
    if (netId == 0) {
        return false;
    }
    out.resize(kPlayerGoneSize);
    out[0] = kPlayerGoneType;
    putU32(out.data() + 1, netId);
    return true;
}

bool decodePlayerGone(const std::uint8_t* data, std::size_t size,
                      std::uint32_t& netId) {
    if (data == nullptr || size != kPlayerGoneSize || data[0] != kPlayerGoneType) {
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