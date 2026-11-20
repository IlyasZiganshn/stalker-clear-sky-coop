#pragma once

#include <cstdint>

namespace coopnet {

// Tracks which sequences arrived recently, to fill the ack/ack_bits fields of
// the packet header. `ackBits` bit i means "sequence ackSequence()-1-i arrived".
class AckState {
public:
    void record(std::uint16_t sequence);

    // Until at least one packet arrives, a zero header must not be sent: it
    // would confirm sequence 0.
    bool hasReceived() const { return hasReceived_; }
    std::uint16_t ackSequence() const { return latest_; }
    std::uint32_t ackBits() const { return received_; }

private:
    std::uint16_t latest_ = 0;
    std::uint32_t received_ = 0;
    bool hasReceived_ = false;
};

}  // namespace coopnet