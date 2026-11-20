#include "coopnet/input.h"

#include <cstddef>
#include <cstdint>

#include "test_framework.h"

using namespace coopnet;

COOP_TEST(input_roundtrip) {
    InputState input;
    input.tick = 123456;
    input.ackSnapshot = 77;
    input.moveX = 0.5f;
    input.moveY = -1.0f;
    input.viewYaw = 3.0f;
    input.viewPitch = -0.7f;
    input.buttons = kButtonFire | kButtonAim | kButtonReload;
    input.weaponSlot = 2;

    const auto bytes = encodeInput(input);
    CHECK_EQ(bytes.size(), kInputByteSize);

    InputState decoded;
    CHECK(decodeInput(bytes.data(), bytes.size(), decoded));
    CHECK_EQ(decoded.tick, input.tick);
    CHECK_EQ(decoded.ackSnapshot, input.ackSnapshot);
    CHECK_NEAR(decoded.moveX, input.moveX, 0.0001);
    CHECK_NEAR(decoded.moveY, input.moveY, 0.0001);
    CHECK_NEAR(decoded.viewYaw, input.viewYaw, 0.0002);
    CHECK_NEAR(decoded.viewPitch, input.viewPitch, 0.0002);
    CHECK_EQ(decoded.buttons, input.buttons);
    CHECK_EQ(decoded.weaponSlot, input.weaponSlot);
}

COOP_TEST(input_clamps_move_and_pitch) {
    InputState input;
    input.moveX = 4.0f;
    input.moveY = -4.0f;
    input.viewPitch = 2.0f;  // beyond +90 degrees

    const auto bytes = encodeInput(input);
    InputState decoded;
    CHECK(decodeInput(bytes.data(), bytes.size(), decoded));
    CHECK_NEAR(decoded.moveX, 1.0f, 0.0001);
    CHECK_NEAR(decoded.moveY, -1.0f, 0.0001);
    CHECK_NEAR(decoded.viewPitch, 1.5708f, 0.001);
}

COOP_TEST(input_rejects_wrong_size) {
    InputState input;
    const auto bytes = encodeInput(input);

    InputState decoded;
    CHECK(!decodeInput(bytes.data(), bytes.size() - 1, decoded));

    std::uint8_t bigger[kInputByteSize + 1] = {};
    CHECK(!decodeInput(bigger, sizeof(bigger), decoded));

    std::uint8_t empty[1] = {};
    CHECK(!decodeInput(empty, 0, decoded));
}