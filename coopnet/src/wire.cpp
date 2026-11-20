#include "coopnet/wire.h"

namespace coopnet {

void PacketHeader::write(BitWriter& writer) const {
    writer.writeU16(protocol);
    writer.writeU8(static_cast<std::uint8_t>(channel));
    writer.writeU8(flags);
    writer.writeU16(sequence);
    writer.writeU16(ack);
    writer.writeU32(ackBits);
}

bool PacketHeader::read(BitReader& reader, PacketHeader& out) {
    out.protocol = reader.readU16();
    out.channel = static_cast<Channel>(reader.readU8());
    out.flags = reader.readU8();
    out.sequence = reader.readU16();
    out.ack = reader.readU16();
    out.ackBits = reader.readU32();
    return reader.ok();
}

}  // namespace coopnet