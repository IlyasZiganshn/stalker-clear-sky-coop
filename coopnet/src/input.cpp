#include "coopnet/input.h"

#include <algorithm>
#include <cmath>

#include "coopnet/bitstream.h"
#include "coopnet/quantize.h"

namespace coopnet {

namespace {

std::uint16_t encodeMove(float value) {
    const float clamped = std::clamp(value, -1.0f, 1.0f);
    const auto scaled = static_cast<std::int32_t>(std::lround(clamped * 32767.0f));
    return static_cast<std::uint16_t>(static_cast<std::int16_t>(scaled));
}

float decodeMove(std::uint16_t value) {
    return static_cast<float>(static_cast<std::int16_t>(value)) / 32767.0f;
}

}  // namespace

std::vector<std::uint8_t> encodeInput(const InputState& input) {
    BitWriter writer(kInputByteSize);
    writer.writeU32(input.tick);
    writer.writeU32(input.ackSnapshot);
    writer.writeU16(encodeMove(input.moveX));
    writer.writeU16(encodeMove(input.moveY));
    writer.writeU16(encodeAngle(input.viewYaw));
    writer.writeU16(encodePitch(input.viewPitch));
    writer.writeU16(input.buttons);
    writer.writeU8(input.weaponSlot);
    return writer.data();
}

bool decodeInput(const std::uint8_t* data, std::size_t size, InputState& out) {
    BitReader reader(data, size);
    InputState input;
    input.tick = reader.readU32();
    input.ackSnapshot = reader.readU32();
    input.moveX = decodeMove(reader.readU16());
    input.moveY = decodeMove(reader.readU16());
    input.viewYaw = decodeAngle(reader.readU16());
    input.viewPitch = decodePitch(reader.readU16());
    input.buttons = reader.readU16();
    input.weaponSlot = reader.readU8();
    if (!reader.ok() || reader.bitPosition() != size * 8) {
        return false;
    }
    out = input;
    return true;
}

}  // namespace coopnet