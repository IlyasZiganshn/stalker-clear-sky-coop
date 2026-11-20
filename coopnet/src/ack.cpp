#include "coopnet/ack.h"

#include "coopnet/sequence.h"

namespace coopnet {

void AckState::record(std::uint16_t sequence) {
    if (!hasReceived_) {
        hasReceived_ = true;
        latest_ = sequence;
        received_ = 0;
        return;
    }

    if (sequenceNewer(sequence, latest_)) {
        const auto diff = static_cast<std::uint16_t>(sequence - latest_);
        if (diff >= 32) {
            received_ = 0;
        } else {
            received_ = (received_ << diff) | (1u << (diff - 1));
        }
        latest_ = sequence;
        return;
    }

    const auto back = static_cast<std::uint16_t>(latest_ - sequence);
    if (back >= 1 && back <= 32) {
        received_ |= 1u << (back - 1);
    }
}

}  // namespace coopnet