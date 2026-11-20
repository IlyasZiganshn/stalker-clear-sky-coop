#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace coopnet {

// Application-level messages that are small, rare and must not be lost, so they
// travel on the control channel. World snapshots and input have their own
// codecs; this layer is for chat, notices and scripted RPC.
//
// Opcodes 1..5 belong to the handshake and are consumed by the peer before the
// application sees them, so application types start at 0x10.
enum class AppMessageType : std::uint8_t {
    Chat = 0x10,
};

struct AppMessage {
    AppMessageType type = AppMessageType::Chat;
    std::uint32_t sender = 0;  // net_id of the author; 0 = host/server
    std::string text;          // UTF-8 bytes, no NUL terminator on the wire
};

constexpr std::size_t kMaxChatBytes = 200;
constexpr std::size_t kMaxAppMessageSize = 256;

// Wire format: [u8 type][u32 sender][u16 length][u8 text[length]].
// Encoding fails for an unknown type or text longer than kMaxChatBytes.
bool encodeAppMessage(const AppMessage& message,
                      std::vector<std::uint8_t>& out);

// Strict decoding: unknown type, empty text, inconsistent or trailing bytes
// are errors.
bool decodeAppMessage(const std::uint8_t* data, std::size_t size,
                      AppMessage& out);

} // namespace coopnet