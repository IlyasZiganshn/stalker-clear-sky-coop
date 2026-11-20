#include "stdafx.h"

#include "coop/coop_bridge.h"

#include <cstdio>
#include <memory>

#include "coopnet/net.h"
#include "coopnet/peer.h"
#include "coopnet/server.h"

namespace
{
constexpr u32 kBuildHash = 0x43534B59; // "CSKY"

struct Bridge
{
    std::unique_ptr<coopnet::NetworkScope> network;
    std::unique_ptr<coopnet::UdpSocket> socket;
    std::unique_ptr<coopnet::NetServer> server;
    std::unique_ptr<coopnet::ClientPeer> client;
    coopnet::Endpoint serverEndpoint;
    u16 port = 0;
    char status[192] = "coop: off";
};

Bridge& state()
{
    static Bridge instance;
    return instance;
}

coopnet::PeerConfig makePeerConfig()
{
    coopnet::PeerConfig config;
    config.buildHash = kBuildHash;
    config.maxPlayers = 4;
    return config;
}

pcstr clientStateName(coopnet::PeerState peerState)
{
    switch (peerState)
    {
    case coopnet::PeerState::Idle: return "idle";
    case coopnet::PeerState::HelloSent: return "connecting";
    case coopnet::PeerState::Acked: return "joining";
    case coopnet::PeerState::HelloReceived: return "handshake";
    case coopnet::PeerState::Joined: return "joined";
    case coopnet::PeerState::Denied: return "denied";
    }
    return "unknown";
}
} // namespace

namespace coop
{
void BridgeStop()
{
    Bridge& b = state();
    b.client.reset();
    b.server.reset();
    b.socket.reset();
    b.network.reset();
    b.port = 0;
    std::snprintf(b.status, sizeof(b.status), "coop: off");
}

bool BridgeStartServer(u16 port)
{
    BridgeStop();

    Bridge& b = state();
    b.network = std::make_unique<coopnet::NetworkScope>();
    if (!b.network->ok())
    {
        std::snprintf(b.status, sizeof(b.status), "coop: network init failed");
        return false;
    }

    b.socket = std::make_unique<coopnet::UdpSocket>();
    if (!b.socket->open(port))
    {
        BridgeStop();
        std::snprintf(b.status, sizeof(b.status), "coop: cannot bind port %u", unsigned(port));
        return false;
    }

    coopnet::NetServerConfig config;
    config.peer = makePeerConfig();
    b.server = std::make_unique<coopnet::NetServer>(config);
    b.port = b.socket->localPort();
    std::snprintf(b.status, sizeof(b.status), "coop: hosting on port %u", unsigned(b.port));
    return true;
}

bool BridgeStartClient(pcstr host, u16 port)
{
    BridgeStop();

    Bridge& b = state();
    coopnet::Endpoint endpoint;
    const bool loopback = host != nullptr && xr_stricmp(host, "localhost") == 0;
    if (host == nullptr || (!loopback && !coopnet::Endpoint::parse(host, port, endpoint)))
    {
        std::snprintf(b.status, sizeof(b.status), "coop: bad address '%s'", host ? host : "");
        return false;
    }
    if (loopback)
        endpoint = coopnet::Endpoint::loopback(port);

    b.network = std::make_unique<coopnet::NetworkScope>();
    if (!b.network->ok())
    {
        std::snprintf(b.status, sizeof(b.status), "coop: network init failed");
        return false;
    }

    b.socket = std::make_unique<coopnet::UdpSocket>();
    if (!b.socket->open())
    {
        BridgeStop();
        std::snprintf(b.status, sizeof(b.status), "coop: cannot open client socket");
        return false;
    }

    b.client = std::make_unique<coopnet::ClientPeer>(makePeerConfig());
    if (!b.client->start("player"))
    {
        BridgeStop();
        std::snprintf(b.status, sizeof(b.status), "coop: cannot start handshake");
        return false;
    }

    b.serverEndpoint = endpoint;
    std::snprintf(b.status, sizeof(b.status), "coop: connecting to %s", endpoint.toString().c_str());
    return true;
}

void BridgeTick(u32 nowMs)
{
    Bridge& b = state();
    if (!b.socket)
        return;

    std::uint8_t buffer[2048];
    for (;;)
    {
        coopnet::Endpoint from;
        const std::int32_t received = b.socket->receiveFrom(buffer, sizeof(buffer), from);
        if (received < 0)
            break;
        if (b.server)
            b.server->onDatagram(from, buffer, static_cast<std::size_t>(received), nowMs);
        else if (b.client)
            b.client->onDatagram(buffer, static_cast<std::size_t>(received), nowMs);
    }

    if (b.server)
    {
        for (const auto& outgoing : b.server->poll(nowMs))
            b.socket->sendTo(outgoing.to, outgoing.data.data(), outgoing.data.size());
        // M1: transport only; application payloads are drained and ignored.
        b.server->takeDelivered();
        std::snprintf(b.status, sizeof(b.status), "coop: hosting on port %u, peers %llu, joined %llu",
            unsigned(b.port),
            static_cast<unsigned long long>(b.server->peerCount()),
            static_cast<unsigned long long>(b.server->joinedCount()));
        return;
    }

    if (b.client)
    {
        for (const auto& datagram : b.client->poll(nowMs))
            b.socket->sendTo(b.serverEndpoint, datagram.data(), datagram.size());
        b.client->takeDelivered();

        if (b.client->state() == coopnet::PeerState::Joined)
            std::snprintf(b.status, sizeof(b.status), "coop: joined %s, rtt %u ms",
                b.serverEndpoint.toString().c_str(), unsigned(b.client->session().rttMs()));
        else if (b.client->timedOut(nowMs))
            std::snprintf(b.status, sizeof(b.status), "coop: connection timed out");
        else
            std::snprintf(b.status, sizeof(b.status), "coop: %s %s",
                clientStateName(b.client->state()), b.serverEndpoint.toString().c_str());
    }
}

bool BridgeActive()
{
    Bridge& b = state();
    return b.server != nullptr || b.client != nullptr;
}

u32 BridgePeerCount()
{
    Bridge& b = state();
    if (b.server)
        return static_cast<u32>(b.server->joinedCount());
    if (b.client && b.client->state() == coopnet::PeerState::Joined)
        return 1;
    return 0;
}

pcstr BridgeStatus()
{
    return state().status;
}
} // namespace coop