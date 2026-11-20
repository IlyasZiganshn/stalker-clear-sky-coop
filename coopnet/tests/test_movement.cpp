#include "coopnet/client_predictor.h"
#include "coopnet/movement.h"

#include <cstdint>

#include "test_framework.h"

namespace {

constexpr float kDt = 1.0f / 60.0f;

coopnet::InputState forwardInput(std::uint32_t tick) {
    coopnet::InputState input;
    input.tick = tick;
    input.moveY = 1.0f;
    return input;
}

void stepFor(coopnet::MoveState& state, const coopnet::InputState& input,
             int steps) {
    for (int i = 0; i < steps; ++i) {
        coopnet::stepMovement(state, input, coopnet::MovementConfig{}, kDt);
    }
}

}  // namespace

COOP_TEST(movement_forward_follows_yaw) {
    coopnet::MoveState state;
    coopnet::InputState input = forwardInput(1);
    input.viewYaw = 0.0f;
    stepFor(state, input, 120);

    CHECK(state.position.z > 6.0f);
    CHECK_NEAR(state.position.x, 0.0f, 1e-5);
    CHECK_NEAR(state.position.y, 0.0f, 1e-6);

    // Yaw of 90 degrees turns forward towards +x.
    coopnet::MoveState turned;
    input.viewYaw = 1.5707963f;
    stepFor(turned, input, 120);
    CHECK(turned.position.x > 6.0f);
    CHECK_NEAR(turned.position.z, 0.0f, 1e-3);
}

COOP_TEST(movement_is_deterministic) {
    coopnet::MoveState first;
    coopnet::MoveState second;
    for (std::uint32_t tick = 1; tick <= 240; ++tick) {
        coopnet::InputState input = forwardInput(tick);
        input.moveX = (tick % 7 == 0) ? 0.5f : 0.0f;
        input.viewYaw = 0.3f * static_cast<float>(tick % 5);
        if (tick % 50 == 0) {
            input.buttons = coopnet::kButtonJump;
        }
        coopnet::stepMovement(first, input, coopnet::MovementConfig{}, kDt);
        coopnet::stepMovement(second, input, coopnet::MovementConfig{}, kDt);
    }
    CHECK(first.position.x == second.position.x);
    CHECK(first.position.y == second.position.y);
    CHECK(first.position.z == second.position.z);
    CHECK(first.velocity.x == second.velocity.x);
    CHECK(first.velocity.y == second.velocity.y);
    CHECK(first.velocity.z == second.velocity.z);
}

COOP_TEST(movement_jump_and_land) {
    coopnet::MoveState state;
    coopnet::InputState jump = forwardInput(1);
    jump.buttons = coopnet::kButtonJump;
    coopnet::stepMovement(state, jump, coopnet::MovementConfig{}, kDt);
    CHECK(!state.onGround);
    CHECK(state.velocity.y > 0.0f);
    CHECK(state.position.y > 0.0f);

    const float airSpeed = state.velocity.y;
    coopnet::stepMovement(state, jump, coopnet::MovementConfig{}, kDt);
    CHECK(state.velocity.y < airSpeed);  // no double jump

    bool landed = false;
    for (int i = 0; i < 120 && !landed; ++i) {
        coopnet::stepMovement(state, forwardInput(2), coopnet::MovementConfig{},
                              kDt);
        landed = state.onGround;
    }
    CHECK(landed);
    CHECK_NEAR(state.position.y, 0.0f, 1e-6);
}

COOP_TEST(movement_sprint_and_crouch_change_speed) {
    coopnet::MoveState walk;
    coopnet::MoveState sprint;
    coopnet::MoveState crouch;
    coopnet::InputState walkInput = forwardInput(1);
    coopnet::InputState sprintInput = walkInput;
    sprintInput.buttons = coopnet::kButtonSprint;
    coopnet::InputState crouchInput = walkInput;
    crouchInput.buttons = coopnet::kButtonCrouch;

    stepFor(walk, walkInput, 120);
    stepFor(sprint, sprintInput, 120);
    stepFor(crouch, crouchInput, 120);

    CHECK(sprint.position.z > walk.position.z + 3.0f);
    CHECK(crouch.position.z < walk.position.z);
}

COOP_TEST(predictor_reconcile_replays_pending_inputs) {
    const coopnet::MovementConfig config;
    coopnet::ClientPredictor predictor(config, kDt);

    coopnet::MoveState server;
    for (std::uint32_t tick = 1; tick <= 10; ++tick) {
        const coopnet::InputState input = forwardInput(tick);
        predictor.predict(input);
        if (tick <= 5) {
            coopnet::stepMovement(server, input, config, kDt);
        }
    }
    const coopnet::MoveState predicted = predictor.state();

    // The server saw something the client could not predict.
    server.position.x += 0.05f;
    const float error = predictor.reconcile(server, 5);
    CHECK_NEAR(error, 0.05f, 1e-4f);
    CHECK_EQ(predictor.pendingInputs(), std::size_t{5});

    for (std::uint32_t tick = 6; tick <= 10; ++tick) {
        coopnet::stepMovement(server, forwardInput(tick), config, kDt);
    }
    CHECK_NEAR(predictor.state().position.x, server.position.x, 1e-5);
    CHECK_NEAR(predictor.state().position.z, server.position.z, 1e-4);
    CHECK_NEAR(predictor.state().position.x, predicted.position.x + 0.05f,
               1e-4f);
}

COOP_TEST(predictor_snaps_without_replay_on_unknown_ack) {
    coopnet::ClientPredictor predictor(coopnet::MovementConfig{}, kDt);
    for (std::uint32_t tick = 1; tick <= 3; ++tick) {
        predictor.predict(forwardInput(tick));
    }

    coopnet::MoveState far;
    far.position.x = 10.0f;
    far.position.z = -4.0f;
    const float error = predictor.reconcile(far, 777);
    CHECK(error > 1.0f);
    CHECK_EQ(predictor.pendingInputs(), std::size_t{0});
    CHECK_NEAR(predictor.state().position.x, 10.0f, 1e-6);
    CHECK_NEAR(predictor.state().position.z, -4.0f, 1e-6);
}

COOP_TEST(predictor_replays_everything_when_nothing_acked) {
    coopnet::ClientPredictor predictor(coopnet::MovementConfig{}, kDt);
    for (std::uint32_t tick = 1; tick <= 3; ++tick) {
        predictor.predict(forwardInput(tick));
    }

    coopnet::MoveState start;
    predictor.reconcile(start, 0);
    CHECK_EQ(predictor.pendingInputs(), std::size_t{3});
    CHECK(predictor.state().position.z > 0.0f);
}