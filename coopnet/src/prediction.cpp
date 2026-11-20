#include "coopnet/prediction.h"

namespace coopnet {

bool InputHistory::record(const InputState& input) {
    if (hasLastTick_ && input.tick <= lastTick_) {
        return false;
    }
    hasLastTick_ = true;
    lastTick_ = input.tick;
    pending_.push_back(input);
    while (pending_.size() > kMaxPending) {
        pending_.pop_front();
    }
    return true;
}

std::size_t InputHistory::acknowledge(std::uint32_t tick) {
    std::size_t dropped = 0;
    while (!pending_.empty() && pending_.front().tick <= tick) {
        pending_.pop_front();
        ++dropped;
    }
    return dropped;
}

void InputHistory::replay(
    float dtSeconds,
    const std::function<void(const InputState&, float)>& step) const {
    for (const auto& input : pending_) {
        step(input, dtSeconds);
    }
}

void InputHistory::clear() {
    pending_.clear();
    hasLastTick_ = false;
    lastTick_ = 0;
}

}  // namespace coopnet