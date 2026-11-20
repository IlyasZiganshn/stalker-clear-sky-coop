#include "coopnet/client_predictor.h"

#include <cmath>

namespace coopnet {

ClientPredictor::ClientPredictor(MovementConfig config, float dtSeconds)
    : config_(config), dt_(dtSeconds) {}

void ClientPredictor::step(const InputState& input, float dt) {
    stepMovement(state_, input, config_, dt);
}

void ClientPredictor::predict(const InputState& input) {
    history_.record(input);
    step(input, dt_);
}

float ClientPredictor::reconcile(const MoveState& authoritative,
                                 std::uint32_t ackedTick) {
    const MoveState before = state_;
    state_ = authoritative;

    if (ackedTick == 0) {
        // Nothing has been applied on the server yet: replay everything.
        history_.replay(dt_, [this](const InputState& input, float dt) {
            step(input, dt);
        });
    } else if (history_.acknowledge(ackedTick) == 0) {
        // The ack points at an input this client no longer holds: a full
        // resync is the only correct answer.
        history_.clear();
    } else {
        history_.replay(dt_, [this](const InputState& input, float dt) {
            step(input, dt);
        });
    }

    const float dx = state_.position.x - before.position.x;
    const float dy = state_.position.y - before.position.y;
    const float dz = state_.position.z - before.position.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

void ClientPredictor::reset() {
    state_ = MoveState{};
    history_.clear();
}

} // namespace coopnet