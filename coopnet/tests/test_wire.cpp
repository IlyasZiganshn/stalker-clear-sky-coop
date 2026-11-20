#include "coopnet/wire.h"

#include "test_framework.h"

using coopnet::BitReader;
using coopnet::BitWriter;
using coopnet::Channel;
using coopnet::PacketHeader;
using coopnet::sequenceNewer;

COOP_TEST(wire_header_is_twelve_bytes) {
    PacketHeader header;
    BitWriter writer;
    header.write(writer);
    CHECK_EQ(writer.bitSize(), std::size_t{96});
    CHECK_EQ(writer.byteSize(), PacketHeader::kByteSize);
}

COOP_TEST(wire_header_roundtrip) {
    PacketHeader original;
    original.protocol = coopnet::kProtocolVersion;
    original.channel = Channel::Snapshot;
    original.flags = coopnet::kFlagFragment;
    original.sequence = 4321;
    original.ack = 4319;
    original.ackBits = 0xDEADBEEFu;

    BitWriter writer;
    original.write(writer);

    BitReader reader(writer.data().data(), writer.data().size());
    PacketHeader decoded;
    CHECK(PacketHeader::read(reader, decoded));
    CHECK_EQ(decoded.protocol, original.protocol);
    CHECK(decoded.channel == Channel::Snapshot);
    CHECK_EQ(decoded.flags, original.flags);
    CHECK_EQ(decoded.sequence, original.sequence);
    CHECK_EQ(decoded.ack, original.ack);
    CHECK_EQ(decoded.ackBits, original.ackBits);
}

COOP_TEST(wire_header_read_rejects_truncated_input) {
    const std::uint8_t data[] = {0x01, 0x00, 0x02, 0x00, 0x00};
    BitReader reader(data, sizeof(data));
    PacketHeader header;
    CHECK(!PacketHeader::read(reader, header));
}

COOP_TEST(wire_sequence_comparison_wraps) {
    CHECK(sequenceNewer(5, 4));
    CHECK(!sequenceNewer(4, 5));
    CHECK(!sequenceNewer(4, 4));
    CHECK(sequenceNewer(0, 65535));   // wrapped forward
    CHECK(!sequenceNewer(65535, 0));  // 0 is newer
    CHECK(sequenceNewer(32767, 0));
    CHECK(!sequenceNewer(32768, 0));  // half-range tie is treated as old
}