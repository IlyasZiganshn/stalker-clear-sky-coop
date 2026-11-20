#include "coopnet/server.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "test_framework.h"

using namespace coopnet;

namespace {

constexpr std::uint32_t kBuild = 0x5150u;

Endpoint endpointOf(std::uint16_t port) {
    Endpoint endpoint;
    endpoint.address = 0x0A000001u;
    endpoint.port = port;
    return endpoint;
}

PeerConfig clientOptions(std::uint8_t slot = 0xFF) {
    PeerConfig config;
    config.buildHash = kBuild;
    config.slot = slot;
    return config;
}

NetServerConfig serverOptions(std::uint8_t maxPlayers = 4,
                              std::size_t maxPeers = 8) {
    NetServerConfig config;
    config.peer.buildHash = kBuild;
    config.peer.maxPlayers = maxPlayers;
    config.maxPeers = maxPeers;
    return config;
}

struct Link {
    Endpoint endpoint;
    ClientPeer* client;
};

void pump(NetServer& server, const std::vector<Link>& links,
          std::uint32_t nowMs, int rounds = 10) {
    for (int round = 0; round < rounds; ++round) {
        for (const auto& link : links) {
            for (const auto& datagram : link.client->poll(nowMs)) {
                server.onDatagram(link.endpoint, datagram.data(),
                                  datagram.size(), nowMs);
            }
        }
        for (const auto& outgoing : server.poll(nowMs)) {
            for (const auto& link : links) {
                if (outgoing.to == link.endpoint) {
                    link.client->onDatagram(outgoing.data.data(),
                                            outgoing.data.size(), nowMs);
                }
            }
        }
    }
}

std::vector<std::uint8_t> bytesOf(const std::string& text) {
    return std::vector<std::uint8_t>(text.begin(), text.end());
}

}  // namespace

COOP_TEST(server_joins_two_clients_into_distinct_slots) {
    NetServer server(serverOptions(4));
    ClientPeer first(clientOptions());
    ClientPeer second(clientOptions());
    const Endpoint firstAt = endpointOf(5001);
    const Endpoint secondAt = endpointOf(5002);

    CHECK(first.start("First"));
    CHECK(second.start("Second"));
    pump(server, {{firstAt, &first}, {secondAt, &second}}, 0);

    CHECK(first.state() == PeerState::Joined);
    CHECK(second.state() == PeerState::Joined);
    CHECK_EQ(server.joinedCount(), std::size_t{2});
    CHECK(server.isJoined(firstAt));
    CHECK(server.isJoined(secondAt));
    CHECK(first.netId() != second.netId());
    CHECK(first.takeDelivered().empty());
    CHECK(second.takeDelivered().empty());
}

COOP_TEST(server_denies_when_all_slots_are_taken) {
    NetServer server(serverOptions(1));
    ClientPeer first(clientOptions());
    ClientPeer second(clientOptions());
    const Endpoint firstAt = endpointOf(5101);
    const Endpoint secondAt = endpointOf(5102);

    CHECK(first.start("First"));
    CHECK(second.start("Second"));
    pump(server, {{firstAt, &first}, {secondAt, &second}}, 0);

    CHECK(first.state() == PeerState::Joined);
    CHECK(second.state() == PeerState::Denied);
    CHECK(second.denyReason() == JoinDenyReason::ServerFull);
    CHECK_EQ(server.joinedCount(), std::size_t{1});
}

COOP_TEST(server_routes_messages_between_peers) {
    NetServer server(serverOptions());
    ClientPeer a(clientOptions());
    ClientPeer b(clientOptions());
    const Endpoint atA = endpointOf(6001);
    const Endpoint atB = endpointOf(6002);
    const std::vector<Link> links = {{atA, &a}, {atB, &b}};

    CHECK(a.start("A"));
    CHECK(b.start("B"));
    pump(server, links, 0);
    CHECK(a.state() == PeerState::Joined);
    CHECK(b.state() == PeerState::Joined);

    CHECK(a.send(Channel::Events, bytesOf("from-a")));
    pump(server, links, 10);
    const auto delivered = server.takeDelivered();
    CHECK_EQ(delivered.size(), std::size_t{1});
    CHECK(delivered[0].from == atA);
    CHECK(delivered[0].delivery.channel == Channel::Events);
    CHECK(delivered[0].delivery.payload == bytesOf("from-a"));

    CHECK(server.send(atA, Channel::Events, bytesOf("to-a")));
    CHECK(server.send(atB, Channel::Events, bytesOf("to-b")));
    pump(server, links, 20);
    const auto toA = a.takeDelivered();
    const auto toB = b.takeDelivered();
    CHECK_EQ(toA.size(), std::size_t{1});
    CHECK_EQ(toB.size(), std::size_t{1});
    CHECK(toA[0].payload == bytesOf("to-a"));
    CHECK(toB[0].payload == bytesOf("to-b"));

    CHECK_EQ(server.broadcast(Channel::Events, bytesOf("all")), std::size_t{2});
    pump(server, links, 30);
    CHECK(a.takeDelivered()[0].payload == bytesOf("all"));
    CHECK(b.takeDelivered()[0].payload == bytesOf("all"));
}

COOP_TEST(server_ignores_unknown_peers) {
    NetServer server(serverOptions());
    const Endpoint unknown = endpointOf(6100);
    CHECK(!server.isJoined(unknown));
    CHECK(!server.send(unknown, Channel::Events, bytesOf("hello")));
    CHECK_EQ(server.broadcast(Channel::Events, bytesOf("hello")), std::size_t{0});
}

COOP_TEST(server_expires_silent_peers) {
    NetServer server(serverOptions());
    ClientPeer client(clientOptions());
    const Endpoint at = endpointOf(7001);

    CHECK(client.start("Solo"));
    pump(server, {{at, &client}}, 0);
    CHECK(client.state() == PeerState::Joined);
    CHECK_EQ(server.peerCount(), std::size_t{1});

    server.poll(6000);
    CHECK_EQ(server.peerCount(), std::size_t{0});
}

COOP_TEST(server_caps_unknown_peers) {
    NetServer server(serverOptions(4, 1));
    Session raw;
    CHECK(raw.send(Channel::Control, bytesOf("noise")));
    const auto datagrams = raw.poll(0);
    CHECK_EQ(datagrams.size(), std::size_t{1});

    server.onDatagram(endpointOf(8001), datagrams[0].data(),
                      datagrams[0].size(), 0);
    server.onDatagram(endpointOf(8002), datagrams[0].data(),
                      datagrams[0].size(), 0);

    CHECK_EQ(server.peerCount(), std::size_t{1});
    CHECK_EQ(server.droppedDatagrams(), std::size_t{1});
}

COOP_TEST(server_reports_join_and_leave_events) {
    NetServer server(serverOptions());
    ClientPeer client(clientOptions());
    const Endpoint at = endpointOf(8101);

    CHECK(client.start("Event"));
    pump(server, {{at, &client}}, 0);
    CHECK(client.state() == PeerState::Joined);

    auto events = server.takeEvents();
    CHECK_EQ(events.size(), std::size_t{1});
    CHECK(events[0].type == NetServer::ServerEvent::Type::Joined);
    CHECK(events[0].endpoint == at);
    CHECK_EQ(events[0].netId, client.netId());
    CHECK(server.takeEvents().empty());

    server.poll(6000);
    CHECK_EQ(server.peerCount(), std::size_t{0});
    events = server.takeEvents();
    CHECK_EQ(events.size(), std::size_t{1});
    CHECK(events[0].type == NetServer::ServerEvent::Type::Left);
    CHECK_EQ(events[0].netId, client.netId());
    CHECK(server.takeEvents().empty());
}