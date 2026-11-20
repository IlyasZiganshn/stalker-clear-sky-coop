#include "coopnet/peer.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "test_framework.h"

using namespace coopnet;

namespace {

std::vector<std::uint8_t> bytesOf(const std::string& text) {
    return std::vector<std::uint8_t>(text.begin(), text.end());
}

void pumpPair(ClientPeer& client, ServerPeer& server, std::uint32_t nowMs,
              int rounds = 8) {
    for (int i = 0; i < rounds; ++i) {
        for (const auto& datagram : client.poll(nowMs)) {
            server.onDatagram(datagram.data(), datagram.size(), nowMs);
        }
        for (const auto& datagram : server.poll(nowMs)) {
            client.onDatagram(datagram.data(), datagram.size(), nowMs);
        }
    }
}

PeerConfig serverOptions(std::uint32_t buildHash) {
    PeerConfig config;
    config.buildHash = buildHash;
    config.sessionToken = 0x11223344u;
    config.maxPlayers = 4;
    config.rules.difficulty = 2;
    return config;
}

PeerConfig clientOptions(std::uint32_t buildHash) {
    PeerConfig config;
    config.buildHash = buildHash;
    return config;
}

}  // namespace

COOP_TEST(peer_completes_handshake) {
    ClientPeer client(clientOptions(0xC0FFEEu));
    ServerPeer server(serverOptions(0xC0FFEEu));

    CHECK(client.start("Strelok"));
    CHECK(client.state() == PeerState::HelloSent);
    CHECK(!client.start("Again"));  // only once
    CHECK(client.takeDelivered().empty());

    pumpPair(client, server, 0);

    CHECK(client.state() == PeerState::Joined);
    CHECK(server.state() == PeerState::Joined);
    CHECK(server.playerName() == "Strelok");
    CHECK_EQ(client.sessionToken(), std::uint32_t{0x11223344u});
    CHECK_EQ(client.netId(), std::uint32_t{1000u});
    CHECK_EQ(server.clientId(), std::uint32_t{1u});
    CHECK_EQ(client.rules().difficulty, std::uint8_t{2});
    CHECK_EQ(client.protocolViolations(), std::uint32_t{0});
    CHECK_EQ(server.protocolViolations(), std::uint32_t{0});

    // Handshake messages must not leak into application traffic.
    CHECK(client.takeDelivered().empty());
    CHECK(server.takeDelivered().empty());
}

COOP_TEST(peer_denies_build_mismatch) {
    ClientPeer client(clientOptions(1));
    ServerPeer server(serverOptions(2));

    CHECK(client.start("Strelok"));
    pumpPair(client, server, 0);

    CHECK(client.state() == PeerState::Denied);
    CHECK(client.denyReason() == JoinDenyReason::BuildMismatch);
    CHECK(server.state() == PeerState::Denied);
    CHECK(!client.send(Channel::Events, bytesOf("shot")));
}

COOP_TEST(peer_denies_when_slot_is_out_of_range) {
    PeerConfig clientConfig;
    clientConfig.slot = 2;
    PeerConfig serverConfig;
    serverConfig.maxPlayers = 2;

    ClientPeer client(clientConfig);
    ServerPeer server(serverConfig);

    CHECK(client.start("Strelok"));
    pumpPair(client, server, 0);

    CHECK(client.state() == PeerState::Denied);
    CHECK(client.denyReason() == JoinDenyReason::ServerFull);
}

COOP_TEST(peer_denies_other_handshake_protocol) {
    ServerPeer server(serverOptions(0));

    // A raw session lets us send a hello whose payload protocol differs from
    // the (matching) header protocol, exercising the server's own check.
    Session raw;
    ClientHello hello;
    hello.protocol = static_cast<std::uint16_t>(kProtocolVersion + 1);
    hello.buildHash = 0;
    hello.name = "Future";
    CHECK(raw.send(Channel::Control, encodeClientHello(hello)));

    for (const auto& datagram : raw.poll(0)) {
        server.onDatagram(datagram.data(), datagram.size(), 0);
    }
    CHECK(server.state() == PeerState::Denied);

    for (const auto& datagram : server.poll(0)) {
        raw.onDatagram(datagram.data(), datagram.size(), 0);
    }
    const auto delivered = raw.takeDelivered();
    CHECK_EQ(delivered.size(), std::size_t{1});
    ServerJoinDeny deny;
    CHECK(decodeServerJoinDeny(delivered[0].payload.data(),
                               delivered[0].payload.size(), deny));
    CHECK(deny.reason == JoinDenyReason::VersionMismatch);
}

COOP_TEST(peer_exchanges_application_messages_after_join) {
    ClientPeer client(clientOptions(7));
    ServerPeer server(serverOptions(7));

    CHECK(!client.send(Channel::Events, bytesOf("too-early")));

    CHECK(client.start("Strelok"));
    pumpPair(client, server, 0);
    CHECK(client.state() == PeerState::Joined);
    CHECK(server.state() == PeerState::Joined);

    CHECK(client.send(Channel::Events, bytesOf("shot")));
    pumpPair(client, server, 10);
    auto toServer = server.takeDelivered();
    CHECK_EQ(toServer.size(), std::size_t{1});
    CHECK(toServer[0].channel == Channel::Events);
    CHECK(toServer[0].payload == bytesOf("shot"));

    CHECK(server.send(Channel::Events, bytesOf("hit")));
    pumpPair(client, server, 20);
    auto toClient = client.takeDelivered();
    CHECK_EQ(toClient.size(), std::size_t{1});
    CHECK(toClient[0].channel == Channel::Events);
    CHECK(toClient[0].payload == bytesOf("hit"));
}

COOP_TEST(peer_reports_silence_after_join) {
    ClientPeer client(clientOptions(0));
    ServerPeer server(serverOptions(0));

    CHECK(!client.timedOut(100000));  // nothing received yet

    CHECK(client.start("Strelok"));
    pumpPair(client, server, 0);
    CHECK(client.state() == PeerState::Joined);

    CHECK(!client.timedOut(4999));
    CHECK(client.timedOut(5000));
    CHECK(server.timedOut(5000));
}