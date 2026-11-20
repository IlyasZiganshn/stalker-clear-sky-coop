#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>

#include "coopnet/input.h"

namespace coopnet {

// Client-side record of inputs that were simulated locally but not yet
// acknowledged by the server. On an authoritative correction the client snaps
// to the server state and replays every input after the acknowledged tick
// through the same movement step it used for prediction.
class InputHistory {
public:
    static constexpr std::size_t kMaxPending = 256;  // ~4 s at 60 Hz

    // Records a simulated input. A tick that is not newer than the last
    // recorded one is ignored and returns false. When the history is full the
    // oldest input is dropped: the link is dead at that point anyway.
    bool record(const InputState& input);

    // Drops inputs up to and including `tick`. Returns how many were dropped;
    // zero means the tick was never recorded, so there is nothing to replay.
    std::size_t acknowledge(std::uint32_t tick);

    // Applies every pending input in tick order through `step(input, dt)`.
    void replay(float dtSeconds,
                const std::function<void(const InputState&, float)>& step) const;

    const std::deque<InputState>& pending() const { return pending_; }
    std::size_t size() const { return pending_.size(); }
    void clear();

private:
    std::deque<InputState> pending_;
    bool hasLastTick_ = false;
    std::uint32_t lastTick_ = 0;
};

}  // namespace coopnet