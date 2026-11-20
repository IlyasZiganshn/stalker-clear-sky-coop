#include "coopnet/movement.h"

#include <cmath>

namespace coopnet {

namespace {

float approach(float value, float target, float maxDelta) {
    const float delta = target - value;
    if (delta > maxDelta) {
        return value + maxDelta;
    }
    if (delta < -maxDelta) {
        return value - maxDelta;
    }
    return target;
}

} // namespace

void stepMovement(MoveState& state, const InputState& input,
                  const MovementConfig& config, float dt) {
    const float fwdX = std::sin(input.viewYaw);
    const float fwdZ = std::cos(input.viewYaw);
    const float rightX = std::cos(input.viewYaw);
    const float rightZ = -std::sin(input.viewYaw);

    float wishX = rightX * input.moveX + fwdX * input.moveY;
    float wishZ = rightZ * input.moveX + fwdZ * input.moveY;
    const float wishLength = std::sqrt(wishX * wishX + wishZ * wishZ);
    if (wishLength > 1.0f) {
        wishX /= wishLength;
        wishZ /= wishLength;
    }

    float speed = config.walkSpeed;
    if (input.buttons & kButtonSprint) {
        speed *= config.sprintMultiplier;
    }
    if (input.buttons & kButtonCrouch) {
        speed *= config.crouchMultiplier;
    }

    const float maxDelta = config.acceleration * dt;
    state.velocity.x = approach(state.velocity.x, wishX * speed, maxDelta);
    state.velocity.z = approach(state.velocity.z, wishZ * speed, maxDelta);

    if ((input.buttons & kButtonJump) && state.onGround) {
        state.velocity.y = config.jumpSpeed;
        state.onGround = false;
    }

    state.velocity.y -= config.gravity * dt;

    state.position.x += state.velocity.x * dt;
    state.position.y += state.velocity.y * dt;
    state.position.z += state.velocity.z * dt;

    if (state.position.y <= config.groundY) {
        state.position.y = config.groundY;
        state.velocity.y = 0.0f;
        state.onGround = true;
    }
}

} // namespace coopnet