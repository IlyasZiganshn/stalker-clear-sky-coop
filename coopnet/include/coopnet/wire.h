#pragma once

#include <cstddef>
#include <cstdint>

#include "coopnet/bitstream.h"
#include "coopnet/sequence.h"

namespace coopnet {

// Bumped on any incompatible change to headers, snapshot layout or RPC table.
constexpr std::uint16_t kProtocolVersion = 3;

// One UDP socket, four logical channels. See docs/PROTOCOL.md.
enum class Channel : std::uint8_t {
    Control = 0,   // reliable ordered: handshake, level load, script RPC
    Events = 1,    // reliable unordered: shots, hits, pickups
    Snapshot = 2,  // unreliable sequenced: world state
    Input = 3,     // unreliable: client input and acks
};

enum PacketFlag : std::uint8_t {
    kFlagReliable = 1u << 0,
    kFlagFragment = 1u << 1,
    kFlagAckOnly = 1u << 2,
    kFlagKeepalive = 1u << 3,
};

// Fixed 12-byte prefix of every datagram.
struct PacketHeader {
    std::uint16_t protocol = kProtocolVersion;
    Channel channel = Channel::Control;
    std::uint8_t flags = 0;
    std::uint16_t sequence = 0;
    std::uint16_t ack = 0;      // highest contiguous sequence received on this channel
    std::uint32_t ackBits = 0;  // bit i = sequence ack-1-i was received

    static constexpr std::size_t kByteSize = 12;

    void write(BitWriter& writer) const;
    static bool read(BitReader& reader, PacketHeader& out);
};

}  // namespace coopnet