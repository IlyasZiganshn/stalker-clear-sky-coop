#include "coopnet/handshake.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "coopnet/bitstream.h"
#include "coopnet/wire.h"
#include "test_framework.h"

using namespace coopnet;

COOP_TEST(handshake_client_hello_roundtrip) {
    ClientHello hello;
    hello.protocol = kProtocolVersion;
    hello.buildHash = 0xDEADBEEFu;
    hello.name = "Strelok";
    hello.token = 0x12345678u;

    const auto bytes = encodeClientHello(hello);
    CHECK(!bytes.empty());
    CHECK_EQ(bytes[0], std::uint8_t{static_cast<std::uint8_t>(
                           HandshakeOpcode::ClientHello)});

    ClientHello decoded;
    CHECK(decodeClientHello(bytes.data(), bytes.size(), decoded));
    CHECK_EQ(decoded.protocol, hello.protocol);
    CHECK_EQ(decoded.buildHash, hello.buildHash);
    CHECK(decoded.name == hello.name);
    CHECK_EQ(decoded.token, hello.token);
}

COOP_TEST(handshake_name_length_is_bounded) {
    ClientHello hello;
    hello.name = std::string(kMaxNameBytes, 'n');
    const auto bytes = encodeClientHello(hello);
    CHECK(!bytes.empty());
    ClientHello decoded;
    CHECK(decodeClientHello(bytes.data(), bytes.size(), decoded));
    CHECK(decoded.name == hello.name);

    hello.name = std::string(kMaxNameBytes + 1, 'n');
    CHECK(encodeClientHello(hello).empty());

    hello.name.clear();
    const auto empty = encodeClientHello(hello);
    CHECK(!empty.empty());
    CHECK(decodeClientHello(empty.data(), empty.size(), decoded));
    CHECK(decoded.name.empty());
}

COOP_TEST(handshake_client_hello_rejects_oversized_name_on_wire) {
    const char name[] = "0123456789012345678901234567890123";  // 34 bytes
    BitWriter writer;
    writer.writeU8(static_cast<std::uint8_t>(HandshakeOpcode::ClientHello));
    writer.writeU16(kProtocolVersion);
    writer.writeU32(1);
    writer.writeU16(34);
    writer.alignToByte();
    writer.writeBytes(name, 34);
    writer.writeU32(0);

    ClientHello decoded;
    CHECK(!decodeClientHello(writer.data().data(), writer.data().size(),
                             decoded));
}

COOP_TEST(handshake_hello_ack_roundtrip) {
    ServerHelloAck ack;
    ack.clientId = 7;
    ack.session = 0xAABBCCDDu;
    ack.snapshotHz = 20;
    ack.tickHz = 60;

    const auto bytes = encodeServerHelloAck(ack);
    ServerHelloAck decoded;
    CHECK(decodeServerHelloAck(bytes.data(), bytes.size(), decoded));
    CHECK_EQ(decoded.clientId, ack.clientId);
    CHECK_EQ(decoded.session, ack.session);
    CHECK_EQ(decoded.snapshotHz, ack.snapshotHz);
    CHECK_EQ(decoded.tickHz, ack.tickHz);
}

COOP_TEST(handshake_join_roundtrip) {
    ClientJoin join;
    join.slot = 1;

    const auto bytes = encodeClientJoin(join);
    ClientJoin decoded;
    CHECK(decodeClientJoin(bytes.data(), bytes.size(), decoded));
    CHECK_EQ(decoded.slot, join.slot);

    ServerJoinOk ok;
    ok.netId = 0x10203040u;
    ok.rules.difficulty = 2;
    ok.rules.maxPlayers = 4;
    ok.rules.friendlyFire = true;
    ok.rules.timeSpeedPercent = 75;

    const auto okBytes = encodeServerJoinOk(ok);
    ServerJoinOk decodedOk;
    CHECK(decodeServerJoinOk(okBytes.data(), okBytes.size(), decodedOk));
    CHECK_EQ(decodedOk.netId, ok.netId);
    CHECK_EQ(decodedOk.rules.difficulty, ok.rules.difficulty);
    CHECK_EQ(decodedOk.rules.maxPlayers, ok.rules.maxPlayers);
    CHECK(decodedOk.rules.friendlyFire == ok.rules.friendlyFire);
    CHECK_EQ(decodedOk.rules.timeSpeedPercent, ok.rules.timeSpeedPercent);
}

COOP_TEST(handshake_join_deny_roundtrip) {
    for (const auto reason :
         {JoinDenyReason::VersionMismatch, JoinDenyReason::BuildMismatch,
          JoinDenyReason::ServerFull, JoinDenyReason::BadName,
          JoinDenyReason::Busy, JoinDenyReason::Banned}) {
        ServerJoinDeny deny;
        deny.reason = reason;
        const auto bytes = encodeServerJoinDeny(deny);
        ServerJoinDeny decoded;
        CHECK(decodeServerJoinDeny(bytes.data(), bytes.size(), decoded));
        CHECK(decoded.reason == reason);
    }
}

COOP_TEST(handshake_rejects_wrong_opcode) {
    ClientJoin join;
    join.slot = 0;
    const auto bytes = encodeClientJoin(join);

    ServerJoinDeny deny;
    CHECK(!decodeServerJoinDeny(bytes.data(), bytes.size(), deny));
    ServerHelloAck ack;
    CHECK(!decodeServerHelloAck(bytes.data(), bytes.size(), ack));
}

COOP_TEST(handshake_rejects_truncated_messages) {
    ClientHello hello;
    hello.name = "Ghost";
    const auto bytes = encodeClientHello(hello);
    CHECK(bytes.size() > 1);

    ClientHello decoded;
    CHECK(!decodeClientHello(bytes.data(), bytes.size() - 1, decoded));
}

COOP_TEST(handshake_rejects_trailing_bytes) {
    ClientJoin join;
    join.slot = 3;
    auto bytes = encodeClientJoin(join);
    bytes.push_back(0xFF);

    ClientJoin decoded;
    CHECK(!decodeClientJoin(bytes.data(), bytes.size(), decoded));
}

COOP_TEST(handshake_rejects_unknown_deny_reason) {
    BitWriter writer;
    writer.writeU8(static_cast<std::uint8_t>(HandshakeOpcode::ServerJoinDeny));
    writer.writeU8(99);

    ServerJoinDeny decoded;
    CHECK(!decodeServerJoinDeny(writer.data().data(), writer.data().size(),
                                decoded));
}