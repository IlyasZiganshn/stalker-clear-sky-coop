#include "coopnet/app_message.h"

#include <cstring>

namespace coopnet {

namespace {

void putU16(std::uint8_t* out, std::uint16_t value) {
    out[0] = static_cast<std::uint8_t>(value & 0xFF);
    out[1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
}

void putU32(std::uint8_t* out, std::uint32_t value) {
    out[0] = static_cast<std::uint8_t>(value & 0xFF);
    out[1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
    out[2] = static_cast<std::uint8_t>((value >> 16) & 0xFF);
    out[3] = static_cast<std::uint8_t>((value >> 24) & 0xFF);
}

std::uint16_t getU16(const std::uint8_t* in) {
    return static_cast<std::uint16_t>(in[0] | (in[1] << 8));
}

std::uint32_t getU32(const std::uint8_t* in) {
    return static_cast<std::uint32_t>(in[0]) |
           (static_cast<std::uint32_t>(in[1]) << 8) |
           (static_cast<std::uint32_t>(in[2]) << 16) |
           (static_cast<std::uint32_t>(in[3]) << 24);
}

} // namespace

bool encodeAppMessage(const AppMessage& message,
                      std::vector<std::uint8_t>& out) {
    if (message.type != AppMessageType::Chat) {
        return false;
    }
    if (message.text.size() > kMaxChatBytes) {
        return false;
    }
    out.resize(1 + 4 + 2 + message.text.size());
    out[0] = static_cast<std::uint8_t>(message.type);
    putU32(out.data() + 1, message.sender);
    putU16(out.data() + 5, static_cast<std::uint16_t>(message.text.size()));
    if (!message.text.empty()) {
        std::memcpy(out.data() + 7, message.text.data(), message.text.size());
    }
    return true;
}

bool decodeAppMessage(const std::uint8_t* data, std::size_t size,
                      AppMessage& out) {
    if (data == nullptr || size < 7) {
        return false;
    }
    const auto type = static_cast<AppMessageType>(data[0]);
    if (type != AppMessageType::Chat) {
        return false;
    }
    const std::uint16_t length = getU16(data + 5);
    if (length == 0 || length > kMaxChatBytes) {
        return false;
    }
    if (size != 7 + static_cast<std::size_t>(length)) {
        return false;
    }
    out.type = type;
    out.sender = getU32(data + 1);
    out.text.assign(reinterpret_cast<const char*>(data + 7), length);
    return true;
}

} // namespace coopnet