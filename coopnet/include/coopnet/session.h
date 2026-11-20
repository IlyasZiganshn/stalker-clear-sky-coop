#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

#include "coopnet/ack.h"
#include "coopnet/fragment.h"
#include "coopnet/reliable.h"
#include "coopnet/wire.h"

namespace coopnet {

struct SessionConfig {
    std::uint16_t protocol = kProtocolVersion;
    std::size_t mtu = 1200;
    std::size_t outboxLimit = 4096;  // datagrams waiting for the send window
};

// One side of a connection: packet framing, channel routing, fragmentation,
// reliability and acknowledgements. Knows nothing about sockets or about the
// game; the owner feeds it datagrams and time.
class Session {
public:
    struct Delivery {
        Channel channel;
        std::vector<std::uint8_t> payload;
    };

    struct Stats {
        std::uint64_t sentDatagrams = 0;
        std::uint64_t receivedDatagrams = 0;
        std::uint64_t protocolErrors = 0;
        std::uint64_t malformed = 0;
        std::uint64_t droppedDatagrams = 0;
    };

    explicit Session(SessionConfig config = {});

    // Incoming datagram from the peer. Never throws; bad input is counted.
    void onDatagram(const std::uint8_t* data, std::size_t size, std::uint32_t nowMs);

    // Outgoing datagrams: retransmissions, fresh messages, then ack-only packets.
    std::vector<std::vector<std::uint8_t>> poll(std::uint32_t nowMs);

    // False when the channel cannot accept the payload right now (retry later).
    bool send(Channel channel, std::vector<std::uint8_t> payload);

    std::vector<Delivery> takeDelivered();

    // Messages waiting for acknowledgement on a reliable channel.
    std::size_t inFlight(Channel channel) const;
    std::size_t outboxSize(Channel channel) const;

    const Stats& stats() const { return stats_; }

private:
    struct ChannelState {
        struct OutboxEntry {
            std::vector<std::uint8_t> payload;
            std::uint8_t flags = 0;
        };

        bool reliable = false;
        ReliableSender sender;
        ReliableReceiver receiver;
        AckState ack;
        Reassembler reassembler;
        std::deque<OutboxEntry> outbox;
        std::uint16_t nextUnreliableSend = 0;
        std::uint16_t lastUnreliableReceived = 0;
        bool hasUnreliableReceived = false;
        std::uint16_t lastSentSequence = 0;
        bool ackDirty = false;
    };

    static constexpr std::size_t kChannelCount = 4;

    ChannelState& state(Channel channel);
    const ChannelState& state(Channel channel) const;

    void emit(std::vector<std::vector<std::uint8_t>>& out, Channel channel,
              ChannelState& channelState, std::uint16_t sequence,
              const std::vector<std::uint8_t>& payload, std::uint8_t flags);

    SessionConfig config_;
    std::array<ChannelState, kChannelCount> channels_{};
    std::vector<Delivery> delivered_;
    std::uint32_t nextMessageId_ = 1;
    Stats stats_;
};

}  // namespace coopnet