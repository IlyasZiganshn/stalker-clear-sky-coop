#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

#include "coopnet/ack.h"

namespace coopnet {

struct OutgoingMessage {
    std::uint16_t sequence = 0;
    std::uint8_t flags = 0;
    std::vector<std::uint8_t> payload;
};

// Delivered in order, with the packet flags carried alongside the payload so
// the caller can tell a fragment from a plain message.
struct ReceivedMessage {
    std::uint8_t flags = 0;
    std::vector<std::uint8_t> payload;
};

// Reliable ordered sender on top of an unreliable datagram link.
// Retransmits what the peer has not acknowledged, with exponential backoff.
class ReliableSender {
public:
    static constexpr std::uint32_t kInitialRtoMs = 100;
    static constexpr std::uint32_t kMaxRtoMs = 500;
    // The window bounds both the count and the sequence span: an unacked
    // message must stay inside the 32-bit ack mask, otherwise it could never be
    // acknowledged and would be retransmitted forever.
    static constexpr std::uint16_t kWindow = 32;
    static constexpr std::uint16_t kMaxSpan = 32;

    // False when the send window is full; caller must retry later.
    bool send(std::vector<std::uint8_t> payload, std::uint32_t nowMs,
              std::uint16_t& outSequence, std::uint8_t flags = 0);

    // `nowMs` feeds the RTT estimate; the sender works without it, just
    // without measurements.
    void onAck(std::uint16_t ack, std::uint32_t ackBits,
               std::uint32_t nowMs = 0);

    // Messages to (re)transmit at `nowMs`; also advances retransmit timers.
    std::vector<OutgoingMessage> collect(std::uint32_t nowMs);

    std::size_t pending() const { return queue_.size(); }
    std::uint16_t nextSequence() const { return nextSequence_; }

    // Measured across acknowledgements; 0 until the first usable sample.
    std::uint32_t lastRttMs() const { return lastRttMs_; }
    std::uint32_t smoothedRttMs() const { return smoothedRttMs_; }

private:
    struct Entry {
        std::uint16_t sequence = 0;
        std::uint8_t flags = 0;
        std::vector<std::uint8_t> payload;
        std::uint32_t firstSendMs = 0;
        std::uint32_t nextSendMs = 0;
        std::uint32_t intervalMs = kInitialRtoMs;
    };

    std::vector<Entry> queue_;
    std::uint16_t nextSequence_ = 0;
    std::uint32_t lastRttMs_ = 0;
    std::uint32_t smoothedRttMs_ = 0;
};

// Reliable ordered receiver: buffers out-of-order packets, drops duplicates.
class ReliableReceiver {
public:
    // Accepts at most kWindow sequences ahead of the expected one, matching the
    // sender window so that every outstanding message fits the ack mask.
    static constexpr std::uint16_t kWindow = 32;

    // Messages that became deliverable, in order.
    std::vector<ReceivedMessage> onPacket(std::uint16_t sequence, std::uint8_t flags,
                                          std::vector<std::uint8_t> payload);

    // Acknowledge fields for the next outgoing header. Do not send them before
    // hasReceived() is true: otherwise a zero header would confirm sequence 0.
    bool hasReceived() const { return ack_.hasReceived(); }
    std::uint16_t ackSequence() const { return ack_.ackSequence(); }
    std::uint32_t ackBits() const { return ack_.ackBits(); }

    std::size_t buffered() const { return pending_.size(); }
    std::uint16_t expected() const { return expected_; }

private:
    struct Pending {
        std::uint8_t flags = 0;
        std::vector<std::uint8_t> payload;
    };

    std::uint16_t expected_ = 0;
    AckState ack_;
    std::map<std::uint16_t, Pending> pending_;
};

}  // namespace coopnet