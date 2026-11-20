#include "coopnet/handshake.h"

#include "coopnet/bitstream.h"

namespace coopnet {

namespace {

void writeString(BitWriter& writer, const std::string& text) {
    writer.writeU16(static_cast<std::uint16_t>(text.size()));
    writer.alignToByte();
    if (!text.empty()) {
        writer.writeBytes(text.data(), text.size());
    }
}

bool readString(BitReader& reader, std::string& out) {
    const std::uint16_t length = reader.readU16();
    if (!reader.ok() || length > kMaxNameBytes) {
        return false;
    }
    out.resize(length);
    reader.alignToByte();
    if (length != 0 && !reader.readBytes(out.data(), length)) {
        return false;
    }
    return true;
}

bool finished(const BitReader& reader, std::size_t size) {
    return reader.ok() && reader.bitPosition() == size * 8;
}

}  // namespace

std::vector<std::uint8_t> encodeClientHello(const ClientHello& message) {
    if (message.name.size() > kMaxNameBytes) {
        return {};
    }
    BitWriter writer;
    writer.writeU8(static_cast<std::uint8_t>(HandshakeOpcode::ClientHello));
    writer.writeU16(message.protocol);
    writer.writeU32(message.buildHash);
    writeString(writer, message.name);
    writer.writeU32(message.token);
    return writer.data();
}

bool decodeClientHello(const std::uint8_t* data, std::size_t size,
                       ClientHello& out) {
    BitReader reader(data, size);
    if (reader.readU8() != static_cast<std::uint8_t>(HandshakeOpcode::ClientHello)) {
        return false;
    }
    ClientHello message;
    message.protocol = reader.readU16();
    message.buildHash = reader.readU32();
    if (!readString(reader, message.name)) {
        return false;
    }
    message.token = reader.readU32();
    if (!finished(reader, size)) {
        return false;
    }
    out = std::move(message);
    return true;
}

std::vector<std::uint8_t> encodeServerHelloAck(const ServerHelloAck& message) {
    BitWriter writer;
    writer.writeU8(static_cast<std::uint8_t>(HandshakeOpcode::ServerHelloAck));
    writer.writeU32(message.clientId);
    writer.writeU32(message.session);
    writer.writeU16(message.snapshotHz);
    writer.writeU16(message.tickHz);
    return writer.data();
}

bool decodeServerHelloAck(const std::uint8_t* data, std::size_t size,
                          ServerHelloAck& out) {
    BitReader reader(data, size);
    if (reader.readU8() !=
        static_cast<std::uint8_t>(HandshakeOpcode::ServerHelloAck)) {
        return false;
    }
    ServerHelloAck message;
    message.clientId = reader.readU32();
    message.session = reader.readU32();
    message.snapshotHz = reader.readU16();
    message.tickHz = reader.readU16();
    if (!finished(reader, size)) {
        return false;
    }
    out = message;
    return true;
}

std::vector<std::uint8_t> encodeClientJoin(const ClientJoin& message) {
    BitWriter writer;
    writer.writeU8(static_cast<std::uint8_t>(HandshakeOpcode::ClientJoin));
    writer.writeU8(message.slot);
    return writer.data();
}

bool decodeClientJoin(const std::uint8_t* data, std::size_t size,
                      ClientJoin& out) {
    BitReader reader(data, size);
    if (reader.readU8() != static_cast<std::uint8_t>(HandshakeOpcode::ClientJoin)) {
        return false;
    }
    ClientJoin message;
    message.slot = reader.readU8();
    if (!finished(reader, size)) {
        return false;
    }
    out = message;
    return true;
}

std::vector<std::uint8_t> encodeServerJoinOk(const ServerJoinOk& message) {
    BitWriter writer;
    writer.writeU8(static_cast<std::uint8_t>(HandshakeOpcode::ServerJoinOk));
    writer.writeU32(message.netId);
    writer.writeU8(message.rules.difficulty);
    writer.writeU8(message.rules.maxPlayers);
    writer.writeBool(message.rules.friendlyFire);
    writer.alignToByte();
    writer.writeU8(message.rules.timeSpeedPercent);
    return writer.data();
}

bool decodeServerJoinOk(const std::uint8_t* data, std::size_t size,
                        ServerJoinOk& out) {
    BitReader reader(data, size);
    if (reader.readU8() !=
        static_cast<std::uint8_t>(HandshakeOpcode::ServerJoinOk)) {
        return false;
    }
    ServerJoinOk message;
    message.netId = reader.readU32();
    message.rules.difficulty = reader.readU8();
    message.rules.maxPlayers = reader.readU8();
    message.rules.friendlyFire = reader.readBool();
    reader.alignToByte();
    message.rules.timeSpeedPercent = reader.readU8();
    if (!finished(reader, size)) {
        return false;
    }
    out = message;
    return true;
}

std::vector<std::uint8_t> encodeServerJoinDeny(const ServerJoinDeny& message) {
    BitWriter writer;
    writer.writeU8(static_cast<std::uint8_t>(HandshakeOpcode::ServerJoinDeny));
    writer.writeU8(static_cast<std::uint8_t>(message.reason));
    return writer.data();
}

bool decodeServerJoinDeny(const std::uint8_t* data, std::size_t size,
                          ServerJoinDeny& out) {
    BitReader reader(data, size);
    if (reader.readU8() !=
        static_cast<std::uint8_t>(HandshakeOpcode::ServerJoinDeny)) {
        return false;
    }
    const std::uint8_t reason = reader.readU8();
    if (!finished(reader, size)) {
        return false;
    }
    switch (static_cast<JoinDenyReason>(reason)) {
        case JoinDenyReason::VersionMismatch:
        case JoinDenyReason::BuildMismatch:
        case JoinDenyReason::ServerFull:
        case JoinDenyReason::BadName:
        case JoinDenyReason::Busy:
        case JoinDenyReason::Banned:
            break;
        default:
            return false;  // unknown reason from a newer peer
    }
    out.reason = static_cast<JoinDenyReason>(reason);
    return true;
}

}  // namespace coopnet