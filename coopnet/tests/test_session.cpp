#include "coopnet/session.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include "test_framework.h"
#include "test_support.h"

using coopnet::Channel;
using coopnet::Session;
using coopnet::SessionConfig;

namespace {

void pump(Session& from, Session& to, std::uint32_t nowMs) {
    for (const auto& datagram : from.poll(nowMs)) {
        to.onDatagram(datagram.data(), datagram.size(), nowMs);
    }
}

std::vector<std::uint8_t> bytesOf(const std::string& text) {
    return std::vector<std::uint8_t>(text.begin(), text.end());
}

}  // namespace

COOP_TEST(session_delivers_reliable_message) {
    Session client;
    Session server;

    CHECK(client.send(Channel::Control, bytesOf("hello")));
    pump(client, server, 0);

    const auto delivered = server.takeDelivered();
    CHECK_EQ(delivered.size(), std::size_t{1});
    CHECK(delivered[0].channel == Channel::Control);
    CHECK(delivered[0].payload == bytesOf("hello"));
}

COOP_TEST(session_reassembles_fragmented_message) {
    SessionConfig config;
    config.mtu = 200;
    Session client(config);
    Session server(config);

    const auto payload = coopnet_test::makeBytes(1000, 21);
    CHECK(client.send(Channel::Control, payload));

    const auto datagrams = client.poll(0);
    CHECK(datagrams.size() > 1);
    for (const auto& datagram : datagrams) {
        CHECK(datagram.size() <= config.mtu);
        server.onDatagram(datagram.data(), datagram.size(), 0);
    }

    const auto delivered = server.takeDelivered();
    CHECK_EQ(delivered.size(), std::size_t{1});
    CHECK(delivered[0].payload == payload);
}

COOP_TEST(session_retransmitted_fragments_keep_their_flag) {
    SessionConfig config;
    config.mtu = 200;
    Session client(config);
    Session server(config);

    const auto payload = coopnet_test::makeBytes(1000, 33);
    CHECK(client.send(Channel::Control, payload));

    const auto first = client.poll(0);
    CHECK(first.size() > 1);

    // Lose one fragment; the rest arrive and are buffered.
    for (std::size_t i = 0; i < first.size(); ++i) {
        if (i == 2) {
            continue;
        }
        server.onDatagram(first[i].data(), first[i].size(), 0);
    }
    CHECK(server.takeDelivered().empty());

    // Retransmission must still be marked as a fragment, otherwise the chunk is
    // delivered as a standalone message.
    for (const auto& datagram : client.poll(100)) {
        server.onDatagram(datagram.data(), datagram.size(), 100);
    }

    const auto delivered = server.takeDelivered();
    CHECK_EQ(delivered.size(), std::size_t{1});
    CHECK(delivered[0].payload == payload);
}

COOP_TEST(session_drops_stale_snapshots) {
    Session client;
    Session server;

    CHECK(client.send(Channel::Snapshot, bytesOf("old")));
    CHECK(client.send(Channel::Snapshot, bytesOf("new")));
    const auto datagrams = client.poll(0);
    CHECK_EQ(datagrams.size(), std::size_t{2});

    // Newest arrives first, the stale one must be dropped.
    server.onDatagram(datagrams[1].data(), datagrams[1].size(), 0);
    server.onDatagram(datagrams[0].data(), datagrams[0].size(), 0);

    const auto delivered = server.takeDelivered();
    CHECK_EQ(delivered.size(), std::size_t{1});
    CHECK(delivered[0].payload == bytesOf("new"));
    CHECK_EQ(server.stats().droppedDatagrams, std::uint64_t{1});
}

COOP_TEST(session_rejects_other_protocol_version) {
    Session client;
    SessionConfig other;
    other.protocol = static_cast<std::uint16_t>(coopnet::kProtocolVersion + 1);
    Session server(other);

    CHECK(client.send(Channel::Control, bytesOf("x")));
    pump(client, server, 0);

    CHECK(server.takeDelivered().empty());
    CHECK_EQ(server.stats().protocolErrors, std::uint64_t{1});
}

COOP_TEST(session_ack_only_stops_retransmission) {
    Session client;
    Session server;

    CHECK(client.send(Channel::Control, bytesOf("once")));
    pump(client, server, 0);
    CHECK_EQ(server.takeDelivered().size(), std::size_t{1});
    CHECK_EQ(client.inFlight(Channel::Control), std::size_t{1});

    // Server has nothing to send, but must acknowledge: that datagram carries
    // no payload and must not trigger an acknowledgement of its own.
    const auto acks = server.poll(10);
    CHECK_EQ(acks.size(), std::size_t{1});
    for (const auto& datagram : acks) {
        client.onDatagram(datagram.data(), datagram.size(), 10);
    }
    CHECK_EQ(client.inFlight(Channel::Control), std::size_t{0});

    CHECK(client.poll(11).empty());
    CHECK(server.poll(11).empty());
}

COOP_TEST(session_measures_round_trip) {
    Session client;
    Session server;
    CHECK_EQ(client.rttMs(), 0u);

    CHECK(client.send(Channel::Control, bytesOf("ping")));
    pump(client, server, 0);
    CHECK_EQ(server.takeDelivered().size(), std::size_t{1});

    // The server answers at 10 ms; the client sees its acknowledgement and
    // derives the round trip from it.
    for (const auto& datagram : server.poll(10)) {
        client.onDatagram(datagram.data(), datagram.size(), 10);
    }
    CHECK_EQ(client.inFlight(Channel::Control), std::size_t{0});
    CHECK_EQ(client.rttMs(), 10u);
}

COOP_TEST(session_survives_lossy_bidirectional_link) {
    Session client;
    Session server;
    coopnet_test::Lcg rng(0x5EEDu);

    constexpr std::size_t kMessages = 25;
    std::size_t nextToSend = 0;

    for (std::uint32_t now = 0; now <= 30000; ++now) {
        while (nextToSend < kMessages) {
            const std::string text = "msg-" + std::to_string(nextToSend);
            if (!client.send(Channel::Control, bytesOf(text))) {
                break;
            }
            ++nextToSend;
        }

        for (const auto& datagram : client.poll(now)) {
            if (!rng.chance(20)) {
                server.onDatagram(datagram.data(), datagram.size(), now);
            }
        }
        for (const auto& datagram : server.poll(now)) {
            if (!rng.chance(20)) {
                client.onDatagram(datagram.data(), datagram.size(), now);
            }
        }

        if (nextToSend == kMessages &&
            client.inFlight(Channel::Control) == 0) {
            break;
        }
    }

    const auto delivered = server.takeDelivered();
    CHECK_EQ(delivered.size(), kMessages);
    for (std::size_t i = 0; i < delivered.size(); ++i) {
        CHECK(delivered[i].payload == bytesOf("msg-" + std::to_string(i)));
    }
    CHECK_EQ(client.inFlight(Channel::Control), std::size_t{0});
    CHECK_EQ(client.outboxSize(Channel::Control), std::size_t{0});
}

COOP_TEST(session_rejects_truncated_and_garbage_datagrams) {
    Session server;

    const std::uint8_t tiny[] = {1, 2, 3};
    server.onDatagram(tiny, sizeof(tiny), 0);
    CHECK_EQ(server.takeDelivered().size(), std::size_t{0});

    const std::uint8_t garbage[16] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                      0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                      0xFF, 0xFF, 0xFF, 0xFF};
    server.onDatagram(garbage, sizeof(garbage), 0);
    CHECK_EQ(server.takeDelivered().size(), std::size_t{0});
    CHECK_EQ(server.stats().malformed + server.stats().protocolErrors,
             std::uint64_t{2});
}

COOP_TEST(session_keepalive_is_sent_when_idle) {
    Session client;
    CHECK(client.poll(999).empty());

    const auto datagrams = client.poll(1000);
    CHECK_EQ(datagrams.size(), std::size_t{1});
    CHECK_EQ(datagrams[0].size(), coopnet::PacketHeader::kByteSize);

    coopnet::BitReader reader(datagrams[0].data(), datagrams[0].size());
    coopnet::PacketHeader header;
    CHECK(coopnet::PacketHeader::read(reader, header));
    CHECK((header.flags & coopnet::kFlagKeepalive) != 0);
    CHECK(header.channel == Channel::Control);
}

COOP_TEST(session_keepalive_is_not_acknowledged) {
    Session client;
    Session server;

    const auto keepalive = client.poll(1000);
    CHECK_EQ(keepalive.size(), std::size_t{1});
    server.onDatagram(keepalive[0].data(), keepalive[0].size(), 1000);
    CHECK(server.hasReceived());
    CHECK(server.takeDelivered().empty());

    // The server may send its own keepalive, but must not acknowledge ours.
    for (const auto& datagram : server.poll(1001)) {
        coopnet::BitReader reader(datagram.data(), datagram.size());
        coopnet::PacketHeader header;
        CHECK(coopnet::PacketHeader::read(reader, header));
        CHECK((header.flags & coopnet::kFlagAckOnly) == 0);
    }
}

COOP_TEST(session_detects_peer_silence) {
    Session server;
    CHECK(!server.hasReceived());
    CHECK(!server.timedOut(100000));

    Session client;
    CHECK(client.send(Channel::Control, bytesOf("x")));
    pump(client, server, 0);
    CHECK(server.hasReceived());
    CHECK(!server.timedOut(4999));
    CHECK(server.timedOut(5000));
}

COOP_TEST(session_keepalive_interval_is_configurable) {
    SessionConfig config;
    config.keepaliveMs = 250;
    Session client(config);

    CHECK(client.poll(249).empty());
    CHECK_EQ(client.poll(250).size(), std::size_t{1});
}

COOP_TEST(session_respects_mtu_budget_for_every_datagram) {
    SessionConfig config;
    config.mtu = 300;
    Session client(config);
    Session server(config);

    for (int i = 0; i < 5; ++i) {
        CHECK(client.send(Channel::Events, coopnet_test::makeBytes(900, 40 + i)));
    }

    const auto datagrams = client.poll(0);
    CHECK(!datagrams.empty());
    for (const auto& datagram : datagrams) {
        CHECK(datagram.size() <= config.mtu);
    }

    for (const auto& datagram : datagrams) {
        server.onDatagram(datagram.data(), datagram.size(), 0);
    }
    const auto delivered = server.takeDelivered();
    CHECK_EQ(delivered.size(), std::size_t{5});
}