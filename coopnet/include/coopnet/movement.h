#pragma once

#include "coopnet/input.h"
#include "coopnet/quantize.h"

namespace coopnet {

// Movement constants shared by the server simulation and client prediction.
// The step below is a stand-in for the engine controller: it keeps the shape
// (deterministic step from InputState) so the network layer can be built and
// tested before the real actor movement is hooked up.
struct MovementConfig {
    float walkSpeed = 4.0f;          // m/s
    float sprintMultiplier = 1.6f;
    float crouchMultiplier = 0.45f;
    float acceleration = 40.0f;      // m/s^2, horizontal only
    float gravity = 20.0f;           // m/s^2
    float jumpSpeed = 6.0f;          // m/s
    float groundY = 0.0f;            // flat floor in the demo
};

struct MoveState {
    Vec3 position;
    Vec3 velocity;
    bool onGround = true;
};

// X-Ray HPB convention: yaw 0 looks along +z, +yaw turns towards +x, y up.
// `moveX` strafes right, `moveY` moves forward; both are clamped to unit
// length. Same config, input and dt must give the same result on both peers.
void stepMovement(MoveState& state, const InputState& input,
                  const MovementConfig& config, float dt);

} // namespace coopnet