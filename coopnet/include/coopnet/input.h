#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace coopnet {

enum InputButton : std::uint16_t {
    kButtonFire = 1u << 0,
    kButtonAim = 1u << 1,
    kButtonJump = 1u << 2,
    kButtonCrouch = 1u << 3,
    kButtonSprint = 1u << 4,
    kButtonUse = 1u << 5,
    kButtonReload = 1u << 6,
};

// One client tick as it goes on the wire (C_INPUT in docs/PROTOCOL.md).
struct InputState {
    std::uint32_t tick = 0;
    std::uint32_t ackSnapshot = 0;  // last snapshot applied by the client
    float moveX = 0.0f;             // [-1, 1], strafe right positive
    float moveY = 0.0f;             // [-1, 1], forward positive
    float viewYaw = 0.0f;           // radians, wraps at 2*pi
    float viewPitch = 0.0f;         // radians, [-pi/2, pi/2]
    std::uint16_t buttons = 0;
    std::uint8_t weaponSlot = 0;
};

constexpr std::size_t kInputByteSize = 19;

std::vector<std::uint8_t> encodeInput(const InputState& input);
bool decodeInput(const std::uint8_t* data, std::size_t size, InputState& out);

}  // namespace coopnet