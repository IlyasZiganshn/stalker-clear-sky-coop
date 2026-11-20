#include "coopnet/app_message.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "test_framework.h"

namespace {

coopnet::AppMessage chat(std::uint32_t sender, const std::string& text) {
    coopnet::AppMessage message;
    message.type = coopnet::AppMessageType::Chat;
    message.sender = sender;
    message.text = text;
    return message;
}

}  // namespace

COOP_TEST(app_message_chat_roundtrip) {
    const coopnet::AppMessage original = chat(1001, "привет, сталкер");
    std::vector<std::uint8_t> wire;
    CHECK(coopnet::encodeAppMessage(original, wire));
    CHECK_EQ(wire.size(), std::size_t{7 + original.text.size()});

    coopnet::AppMessage decoded;
    CHECK(coopnet::decodeAppMessage(wire.data(), wire.size(), decoded));
    CHECK(decoded.type == coopnet::AppMessageType::Chat);
    CHECK_EQ(decoded.sender, std::uint32_t{1001});
    CHECK(decoded.text == original.text);
}

COOP_TEST(app_message_rejects_oversized_text_on_encode) {
    std::vector<std::uint8_t> wire;
    CHECK(!coopnet::encodeAppMessage(
        chat(1, std::string(coopnet::kMaxChatBytes + 1, 'x')), wire));

    std::vector<std::uint8_t> fitting;
    CHECK(coopnet::encodeAppMessage(
        chat(1, std::string(coopnet::kMaxChatBytes, 'x')), fitting));
    CHECK(fitting.size() <= coopnet::kMaxAppMessageSize);
}

COOP_TEST(app_message_rejects_unknown_type_on_encode) {
    coopnet::AppMessage message = chat(1, "hi");
    message.type = static_cast<coopnet::AppMessageType>(99);
    std::vector<std::uint8_t> wire;
    CHECK(!coopnet::encodeAppMessage(message, wire));
}

COOP_TEST(app_message_rejects_malformed_input) {
    const coopnet::AppMessage original = chat(7, "hello");
    std::vector<std::uint8_t> wire;
    CHECK(coopnet::encodeAppMessage(original, wire));

    coopnet::AppMessage decoded;
    CHECK(!coopnet::decodeAppMessage(nullptr, wire.size(), decoded));
    CHECK(!coopnet::decodeAppMessage(wire.data(), 0, decoded));
    CHECK(!coopnet::decodeAppMessage(wire.data(), wire.size() - 1, decoded));

    std::vector<std::uint8_t> trailing = wire;
    trailing.push_back(0);
    CHECK(!coopnet::decodeAppMessage(trailing.data(), trailing.size(), decoded));

    std::vector<std::uint8_t> unknown = wire;
    unknown[0] = 99;
    CHECK(!coopnet::decodeAppMessage(unknown.data(), unknown.size(), decoded));

    // A zero-length chat is meaningless and treated as malformed.
    std::vector<std::uint8_t> empty;
    CHECK(coopnet::encodeAppMessage(chat(7, ""), empty));
    CHECK(!coopnet::decodeAppMessage(empty.data(), empty.size(), decoded));

    // A length far beyond the payload must not be trusted.
    std::vector<std::uint8_t> absurd = wire;
    absurd[5] = 0xFF;
    absurd[6] = 0xFF;
    CHECK(!coopnet::decodeAppMessage(absurd.data(), absurd.size(), decoded));
}

// Handshake opcodes are 1..5 and are consumed by the peer before the
// application sees a delivery; colliding with them swallows the message.
static_assert(static_cast<std::uint8_t>(coopnet::AppMessageType::Chat) > 5,
              "application opcodes must stay outside the handshake range");

COOP_TEST(app_message_text_is_opaque_utf8) {
    // Bytes are preserved exactly, including multi-byte UTF-8 and spaces.
    const std::string text = "Кордон — 25 м, 2/3 живых";
    std::vector<std::uint8_t> wire;
    CHECK(coopnet::encodeAppMessage(chat(0, text), wire));

    coopnet::AppMessage decoded;
    CHECK(coopnet::decodeAppMessage(wire.data(), wire.size(), decoded));
    CHECK(decoded.text == text);
    CHECK_EQ(decoded.text.size(), text.size());
}