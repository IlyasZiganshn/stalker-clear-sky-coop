#pragma once

#include <cstdint>

#include "coopnet/movement.h"
#include "coopnet/prediction.h"

namespace coopnet {

// Own-player prediction: every input is simulated locally and kept until the
// server acknowledges it. When an authoritative state arrives, the client snaps
// to it and replays the inputs the server has not applied yet.
class ClientPredictor {
public:
    ClientPredictor(MovementConfig config, float dtSeconds);

    // Simulates `input` locally and records it for a possible replay.
    void predict(const InputState& input);

    // Snaps to `authoritative` and replays pending inputs newer than
    // `ackedTick`. Returns how far the old prediction was from the server,
    // which is the misprediction the player would have seen without replay.
    float reconcile(const MoveState& authoritative, std::uint32_t ackedTick);

    const MoveState& state() const { return state_; }
    std::size_t pendingInputs() const { return history_.size(); }

    void reset();

private:
    void step(const InputState& input, float dt);

    MovementConfig config_;
    float dt_ = 1.0f / 60.0f;
    MoveState state_{};
    InputHistory history_;
};

} // namespace coopnet