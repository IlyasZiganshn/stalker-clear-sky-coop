#include "stdafx.h"

#include "coop/coop_bridge.h"

#include <cstdio>
#include <memory>
#include <vector>

#include "coopnet/app_message.h"
#include "coopnet/input.h"
#include "coopnet/net.h"
#include "coopnet/peer.h"
#include "coopnet/player_interp.h"
#include "coopnet/player_state.h"
#include "coopnet/server.h"
#include "coopnet/spawn.h"
#include "coopnet/spawn_registry.h"
#include "coopnet/world_info.h"

namespace
{
constexpr u32 kBuildHash = 0x43534B59; // "CSKY"
constexpr u32 kHostNetId = 1;
constexpr u32 kPublishIntervalMs = 50;
constexpr u32 kInputIntervalMs = 16;
constexpr u32 kWorldBroadcastMs = 1000;
constexpr u32 kRemoteTimeoutMs = 2000;
// Spawn blobs are repeated slowly so a reloaded level gets its dolls back.
constexpr u32 kSpawnResendMs = 2000;
// Input is sent at 60 Hz: half a second of silence means the stream is dead.
constexpr u32 kInputTimeoutMs = 500;
// Remote players are rendered this far in the past, between two 20 Hz samples.
constexpr u32 kPlayerInterpDelayMs = 100;
constexpr u32 kMaxRemotes = 8;

struct Bridge
{
    std::unique_ptr<coopnet::NetworkScope> network;
    std::unique_ptr<coopnet::UdpSocket> socket;
    std::unique_ptr<coopnet::NetServer> server;
    std::unique_ptr<coopnet::ClientPeer> client;
    coopnet::Endpoint serverEndpoint;
    u16 port = 0;
    char status[192] = "coop: off";
    char lastChat[256] = "";
    coopnet::PlayerState local;
    u32 localPublishMs = 0;
    u32 nowMs = 0;
    coopnet::PlayerInterpolator remotePlayers;
    coop::PlayerInput remoteInputs[kMaxRemotes];
    u32 remoteInputMs[kMaxRemotes] = {};
    u32 localInputMs = 0;
    u32 inputTick = 0;
    coopnet::WorldInfo localWorld;
    bool hasLocalWorld = false;
    u32 worldBroadcastMs = 0;
    bool worldSent = false;
    coopnet::WorldInfo serverWorld;
    bool serverWorldKnown = false;
    u64 serverClockOffsetMs = 0;
    bool levelMismatch = false;
    char levelMismatchText[128] = "";
    std::vector<std::uint8_t> localSpawn;
    bool localSpawnSent = false;
    u32 spawnResendMs = 0;
    coopnet::SpawnRegistry knownSpawns;
    std::vector<coopnet::Spawn> pendingSpawns;
    std::vector<u32> pendingDespawns;
};

static_assert(coop::kCoopSpawnBlobMax == coopnet::kMaxSpawnBytes,
    "bridge blob limit must match the protocol limit");

static_assert(coop::kInputButtonFire == coopnet::kButtonFire &&
        coop::kInputButtonAim == coopnet::kButtonAim &&
        coop::kInputButtonJump == coopnet::kButtonJump &&
        coop::kInputButtonCrouch == coopnet::kButtonCrouch &&
        coop::kInputButtonSprint == coopnet::kButtonSprint &&
        coop::kInputButtonUse == coopnet::kButtonUse &&
        coop::kInputButtonReload == coopnet::kButtonReload,
    "bridge button mask must match the protocol");

Bridge& state()
{
    static Bridge instance;
    return instance;
}

// Level-scoped state: game ids and spawn blobs die with the level, and remote
// transforms from the old level must not leak into the new one.
void resetLevelState(Bridge& b)
{
    b.local = coopnet::PlayerState{};
    b.localPublishMs = 0;
    b.localSpawn.clear();
    b.localSpawnSent = false;
    b.spawnResendMs = 0;
    b.knownSpawns.clear();
    b.pendingSpawns.clear();
    b.pendingDespawns.clear();
    b.remotePlayers.clear();
    for (u32 i = 0; i < kMaxRemotes; ++i)
    {
        b.remoteInputs[i] = coop::PlayerInput{};
        b.remoteInputMs[i] = 0;
    }
    b.worldSent = false;
}

void rememberChat(const coopnet::AppMessage& message)
{
    Bridge& b = state();
    if (message.sender == 0)
        std::snprintf(b.lastChat, sizeof(b.lastChat), "host: %s", message.text.c_str());
    else
        std::snprintf(b.lastChat, sizeof(b.lastChat), "player %u: %s",
            unsigned(message.sender), message.text.c_str());
}

void rememberRemote(Bridge& b, const coopnet::PlayerState& state, u32 nowMs)
{
    // The interpolator ignores repeats and out-of-order samples on its own.
    b.remotePlayers.push(state, nowMs);
}

void forgetRemote(Bridge& b, u32 netId)
{
    b.remotePlayers.forget(netId);
}

void rememberInput(Bridge& b, const coopnet::InputState& input, u32 netId, u32 nowMs)
{
    coop::PlayerInput sample;
    sample.netId = netId;
    sample.moveX = input.moveX;
    sample.moveY = input.moveY;
    sample.yaw = input.viewYaw;
    sample.pitch = input.viewPitch;
    sample.buttons = input.buttons;
    sample.weaponSlot = input.weaponSlot;

    for (u32 i = 0; i < kMaxRemotes; ++i)
    {
        if (b.remoteInputs[i].netId == netId)
        {
            b.remoteInputs[i] = sample;
            b.remoteInputMs[i] = nowMs;
            return;
        }
    }
    for (u32 i = 0; i < kMaxRemotes; ++i)
    {
        if (b.remoteInputs[i].netId == 0)
        {
            b.remoteInputs[i] = sample;
            b.remoteInputMs[i] = nowMs;
            return;
        }
    }
    u32 oldest = 0;
    for (u32 i = 1; i < kMaxRemotes; ++i)
        if (b.remoteInputMs[i] < b.remoteInputMs[oldest])
            oldest = i;
    b.remoteInputs[oldest] = sample;
    b.remoteInputMs[oldest] = nowMs;
}

void forgetInput(Bridge& b, u32 netId)
{
    for (u32 i = 0; i < kMaxRemotes; ++i)
    {
        if (b.remoteInputs[i].netId == netId)
        {
            b.remoteInputs[i] = coop::PlayerInput{};
            b.remoteInputMs[i] = 0;
        }
    }
}

bool hasRemoteInput(const Bridge& b, u32 netId, u32 nowMs)
{
    for (u32 i = 0; i < kMaxRemotes; ++i)
        if (b.remoteInputs[i].netId == netId && nowMs - b.remoteInputMs[i] <= kInputTimeoutMs)
            return true;
    return false;
}

void removeKnownSpawn(Bridge& b, u32 netId)
{
    b.knownSpawns.remove(netId);
}

// Game side has not seen this spawn yet: keep one copy per player.
void queueSpawn(Bridge& b, const coopnet::Spawn& spawn)
{
    for (auto& pending : b.pendingSpawns)
    {
        if (pending.netId == spawn.netId)
        {
            pending = spawn;
            return;
        }
    }
    b.pendingSpawns.push_back(spawn);
}

void queueDespawn(Bridge& b, u32 netId)
{
    for (auto it = b.pendingSpawns.begin(); it != b.pendingSpawns.end();)
        it = it->netId == netId ? b.pendingSpawns.erase(it) : it + 1;
    b.pendingDespawns.push_back(netId);
}

// Decodes application messages the peer handed over after the handshake.
void handleAppMessage(Bridge& b, const coopnet::Session::Delivery& delivery,
    const coopnet::Endpoint* from, u32 nowMs)
{
    // Input comes on its own unreliable channel and only matters to the host:
    // it is simulated there, never relayed to other peers.
    if (delivery.channel == coopnet::Channel::Input)
    {
        coopnet::InputState input;
        if (b.server != nullptr && from != nullptr &&
            coopnet::decodeInput(delivery.payload.data(), delivery.payload.size(), input))
        {
            const u32 netId = b.server->netIdOf(*from);
            if (netId != 0)
                rememberInput(b, input, netId, nowMs);
        }
        return;
    }

    if (delivery.channel != coopnet::Channel::Control)
        return;

    coopnet::AppMessage message;
    if (coopnet::decodeAppMessage(delivery.payload.data(), delivery.payload.size(), message))
    {
        if (b.server && from != nullptr)
        {
            message.sender = b.server->netIdOf(*from);
            if (message.sender == 0)
                return;
            std::vector<std::uint8_t> wire;
            if (coopnet::encodeAppMessage(message, wire))
                b.server->broadcastExcept(*from, coopnet::Channel::Control, wire);
        }
        rememberChat(message);
        return;
    }

    coopnet::WorldInfo world;
    if (coopnet::decodeWorldInfo(delivery.payload.data(), delivery.payload.size(), world))
    {
        b.serverWorld = world;
        b.serverWorldKnown = true;
        b.serverClockOffsetMs = world.gameTimeMs - nowMs;
        if (b.hasLocalWorld && b.localWorld.levelName != world.levelName)
            b.levelMismatch = true;
        return;
    }

    coopnet::PlayerState playerState;
    if (coopnet::decodePlayerState(delivery.payload.data(), delivery.payload.size(), playerState))
    {
        if (b.server && from != nullptr)
        {
            playerState.netId = b.server->netIdOf(*from);
            if (playerState.netId == 0)
                return;
            rememberRemote(b, playerState, nowMs);
            // A peer with a live input stream is simulated by the host; relaying
            // its own transform would race with the authority.
            if (!hasRemoteInput(b, playerState.netId, nowMs))
            {
                std::vector<std::uint8_t> wire;
                if (coopnet::encodePlayerState(playerState, wire))
                    b.server->broadcastExcept(*from, coopnet::Channel::Control, wire);
            }
            return;
        }
        rememberRemote(b, playerState, nowMs);
        return;
    }

    u32 goneNetId = 0;
    if (coopnet::decodePlayerGone(delivery.payload.data(), delivery.payload.size(), goneNetId))
    {
        forgetRemote(b, goneNetId);
        return;
    }

    coopnet::Spawn spawn;
    if (coopnet::decodeSpawn(delivery.payload.data(), delivery.payload.size(), spawn))
    {
        if (b.server && from != nullptr)
        {
            spawn.netId = b.server->netIdOf(*from);
            if (spawn.netId == 0)
                return;
        }
        else if (b.client && spawn.netId == b.client->netId())
            return;

        // A peer that reloaded the level publishes a fresh blob under the same
        // net_id: the old doll belongs to the previous world and must go.
        const coopnet::SpawnRegistry::Change change = b.knownSpawns.observe(spawn);

        if (b.server && from != nullptr)
        {
            std::vector<std::uint8_t> wire;
            if (coopnet::encodeSpawn(spawn, wire))
                b.server->broadcastExcept(*from, coopnet::Channel::Control, wire);
        }

        if (change == coopnet::SpawnRegistry::Change::Replaced)
            queueDespawn(b, spawn.netId);
        queueSpawn(b, spawn);
        return;
    }

    if (coopnet::decodeSpawnGone(delivery.payload.data(), delivery.payload.size(), goneNetId))
    {
        forgetRemote(b, goneNetId);
        removeKnownSpawn(b, goneNetId);
        queueDespawn(b, goneNetId);
    }
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
    b.lastChat[0] = 0;
    b.local = coopnet::PlayerState{};
    b.localPublishMs = 0;
    b.localInputMs = 0;
    b.inputTick = 0;
    b.localWorld = coopnet::WorldInfo{};
    b.hasLocalWorld = false;
    b.worldBroadcastMs = 0;
    b.worldSent = false;
    b.serverWorld = coopnet::WorldInfo{};
    b.serverWorldKnown = false;
    b.serverClockOffsetMs = 0;
    b.levelMismatch = false;
    b.levelMismatchText[0] = 0;
    resetLevelState(b);
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
    b.nowMs = nowMs;

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
        for (const auto& delivery : b.server->takeDelivered())
            handleAppMessage(b, delivery.delivery, &delivery.from, nowMs);
        for (const auto& event : b.server->takeEvents())
        {
            if (event.type == coopnet::NetServer::ServerEvent::Type::Left)
            {
                forgetRemote(b, event.netId);
                forgetInput(b, event.netId);
                removeKnownSpawn(b, event.netId);
                queueDespawn(b, event.netId);
                std::vector<std::uint8_t> wire;
                if (coopnet::encodePlayerGone(event.netId, wire))
                    b.server->broadcast(coopnet::Channel::Control, wire);
                if (coopnet::encodeSpawnGone(event.netId, wire))
                    b.server->broadcast(coopnet::Channel::Control, wire);
                continue;
            }
            // A fresh peer gets the level, clock and every known spawn right
            // away, so a late joiner sees players who were already in world.
            std::vector<std::uint8_t> wire;
            if (b.hasLocalWorld && coopnet::encodeWorldInfo(b.localWorld, wire))
                b.server->send(event.endpoint, coopnet::Channel::Control, wire);
            if (!b.localSpawn.empty())
            {
                coopnet::Spawn hostSpawn;
                hostSpawn.netId = kHostNetId;
                hostSpawn.data = b.localSpawn;
                if (coopnet::encodeSpawn(hostSpawn, wire))
                    b.server->send(event.endpoint, coopnet::Channel::Control, wire);
            }
            for (const auto& known : b.knownSpawns.all())
            {
                if (coopnet::encodeSpawn(known, wire))
                    b.server->send(event.endpoint, coopnet::Channel::Control, wire);
            }
        }

        if (b.hasLocalWorld &&
            (b.worldBroadcastMs == 0 || nowMs - b.worldBroadcastMs >= kWorldBroadcastMs))
        {
            std::vector<std::uint8_t> wire;
            if (coopnet::encodeWorldInfo(b.localWorld, wire))
            {
                b.server->broadcast(coopnet::Channel::Control, wire);
                b.worldBroadcastMs = nowMs;
            }
        }

        if (b.hasLocalWorld)
        {
            std::snprintf(b.status, sizeof(b.status),
                "coop: hosting on port %u, peers %llu, joined %llu, level %s",
                unsigned(b.port),
                static_cast<unsigned long long>(b.server->peerCount()),
                static_cast<unsigned long long>(b.server->joinedCount()),
                b.localWorld.levelName.c_str());
        }
        else
        {
            std::snprintf(b.status, sizeof(b.status),
                "coop: hosting on port %u, peers %llu, joined %llu",
                unsigned(b.port),
                static_cast<unsigned long long>(b.server->peerCount()),
                static_cast<unsigned long long>(b.server->joinedCount()));
        }
        return;
    }

    if (b.client)
    {
        for (const auto& datagram : b.client->poll(nowMs))
            b.socket->sendTo(b.serverEndpoint, datagram.data(), datagram.size());
        for (const auto& delivery : b.client->takeDelivered())
            handleAppMessage(b, delivery, nullptr, nowMs);

        if (b.client->state() == coopnet::PeerState::Joined)
        {
            // The spawn is resent at a low rate: a peer that reloaded its level
            // loses every doll, and only a fresh blob restores them.
            const bool resendDue = b.spawnResendMs == 0 ||
                nowMs - b.spawnResendMs >= kSpawnResendMs;
            if (!b.localSpawn.empty() && b.client->netId() != 0 &&
                (!b.localSpawnSent || resendDue))
            {
                coopnet::Spawn spawn;
                spawn.netId = b.client->netId();
                spawn.data = b.localSpawn;
                std::vector<std::uint8_t> wire;
                if (coopnet::encodeSpawn(spawn, wire) &&
                    b.client->send(coopnet::Channel::Control, wire))
                {
                    b.localSpawnSent = true;
                    if (resendDue)
                        b.spawnResendMs = nowMs;
                }
            }

            if (!b.worldSent && b.hasLocalWorld)
            {
                std::vector<std::uint8_t> wire;
                if (coopnet::encodeWorldInfo(b.localWorld, wire) &&
                    b.client->send(coopnet::Channel::Control, wire))
                    b.worldSent = true;
            }

            if (b.levelMismatch)
            {
                std::snprintf(b.levelMismatchText, sizeof(b.levelMismatchText),
                    "coop: level mismatch ('%s' local, '%s' server)",
                    b.localWorld.levelName.c_str(),
                    b.serverWorld.levelName.c_str());
                BridgeStop();
                std::snprintf(b.status, sizeof(b.status), "%s", b.levelMismatchText);
                return;
            }

            if (b.serverWorldKnown)
                std::snprintf(b.status, sizeof(b.status),
                    "coop: joined %s, rtt %u ms, level %s",
                    b.serverEndpoint.toString().c_str(),
                    unsigned(b.client->session().rttMs()),
                    b.serverWorld.levelName.c_str());
            else
                std::snprintf(b.status, sizeof(b.status), "coop: joined %s, rtt %u ms",
                    b.serverEndpoint.toString().c_str(), unsigned(b.client->session().rttMs()));
        }
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

bool BridgeSay(pcstr text)
{
    Bridge& b = state();
    if (!text || !text[0])
        return false;

    coopnet::AppMessage message;
    message.text = text;
    std::vector<std::uint8_t> wire;
    if (!coopnet::encodeAppMessage(message, wire))
        return false;

    if (b.client)
    {
        if (b.client->state() != coopnet::PeerState::Joined)
            return false;
        return b.client->send(coopnet::Channel::Control, wire);
    }

    if (b.server)
    {
        rememberChat(message);
        b.server->broadcast(coopnet::Channel::Control, wire);
        return true;
    }
    return false;
}

pcstr BridgeLastChat()
{
    return state().lastChat;
}

void BridgePublishLocalPlayer(float x, float y, float z, float yaw,
    u16 bodyState)
{
    Bridge& b = state();
    if (!b.socket)
        return;
    if (b.nowMs - b.localPublishMs < kPublishIntervalMs)
        return;

    coopnet::PlayerState sample;
    sample.x = x;
    sample.y = y;
    sample.z = z;
    sample.yaw = yaw;
    sample.bodyState = static_cast<std::uint16_t>(bodyState & 0x7FFF);

    std::vector<std::uint8_t> wire;
    if (b.client)
    {
        if (b.client->state() != coopnet::PeerState::Joined)
            return;
        sample.netId = b.client->netId();
        if (!coopnet::encodePlayerState(sample, wire))
            return;
        b.client->send(coopnet::Channel::Control, wire);
    }
    else if (b.server)
    {
        // Peers never get a net_id of their own from a headless host, so the
        // host transform travels under a reserved id outside 1000..1003.
        sample.netId = kHostNetId;
        if (!coopnet::encodePlayerState(sample, wire))
            return;
        b.server->broadcast(coopnet::Channel::Control, wire);
    }
    else
        return;

    b.local = sample;
    b.localPublishMs = b.nowMs;
}

u32 BridgeRemotePlayers(RemotePlayer* out, u32 capacity)
{
    Bridge& b = state();
    if (out == nullptr || capacity == 0)
        return 0;

    coopnet::PlayerState players[kMaxRemotes];
    const u32 count = b.remotePlayers.sample(b.nowMs, kPlayerInterpDelayMs,
        kRemoteTimeoutMs, players, kMaxRemotes);
    for (u32 i = 0; i < count && i < capacity; ++i)
    {
        RemotePlayer& player = out[i];
        player.netId = players[i].netId;
        player.x = players[i].x;
        player.y = players[i].y;
        player.z = players[i].z;
        player.yaw = players[i].yaw;
        player.bodyState = players[i].bodyState;
    }
    return count < capacity ? count : capacity;
}

void BridgePublishLocalInput(float moveX, float moveY, float yaw, float pitch,
    u16 buttons, u8 weaponSlot)
{
    Bridge& b = state();
    // The host's own actor already runs from local input.
    if (!b.client || b.client->state() != coopnet::PeerState::Joined)
        return;
    if (b.nowMs - b.localInputMs < kInputIntervalMs)
        return;

    coopnet::InputState input;
    input.tick = b.inputTick + 1;
    input.moveX = moveX;
    input.moveY = moveY;
    input.viewYaw = yaw;
    input.viewPitch = pitch;
    input.buttons = buttons;
    input.weaponSlot = weaponSlot;

    const std::vector<std::uint8_t> wire = coopnet::encodeInput(input);
    if (!b.client->send(coopnet::Channel::Input, wire))
        return;
    b.inputTick = input.tick;
    b.localInputMs = b.nowMs;
}

u32 BridgeRemoteInputs(PlayerInput* out, u32 capacity)
{
    Bridge& b = state();
    if (out == nullptr || capacity == 0)
        return 0;

    u32 written = 0;
    for (u32 i = 0; i < kMaxRemotes && written < capacity; ++i)
    {
        if (b.remoteInputs[i].netId == 0)
            continue;
        if (b.nowMs - b.remoteInputMs[i] > kInputTimeoutMs)
            continue;
        out[written++] = b.remoteInputs[i];
    }
    return written;
}

void BridgePublishRemoteState(u32 netId, float x, float y, float z, float yaw,
    u16 bodyState)
{
    Bridge& b = state();
    if (b.server == nullptr || netId == 0)
        return;

    coopnet::PlayerState sample;
    sample.netId = netId;
    sample.x = x;
    sample.y = y;
    sample.z = z;
    sample.yaw = yaw;
    sample.bodyState = static_cast<std::uint16_t>(bodyState & 0x7FFF);

    std::vector<std::uint8_t> wire;
    if (coopnet::encodePlayerState(sample, wire))
        b.server->broadcast(coopnet::Channel::Control, wire);
}

bool BridgeSetLocalSpawn(const void* data, u32 size)
{
    Bridge& b = state();
    if (data == nullptr || size == 0 || size > kCoopSpawnBlobMax)
        return false;

    const auto* bytes = static_cast<const std::uint8_t*>(data);
    if (b.localSpawn.size() == size &&
        std::equal(b.localSpawn.begin(), b.localSpawn.end(), bytes))
        return true;

    b.localSpawn.assign(bytes, bytes + size);
    b.localSpawnSent = false;
    b.spawnResendMs = 0;

    // A host may have peers already: the client branch below picks the blob up
    // on its own once it is joined.
    if (b.server)
    {
        coopnet::Spawn hostSpawn;
        hostSpawn.netId = kHostNetId;
        hostSpawn.data = b.localSpawn;
        std::vector<std::uint8_t> wire;
        if (coopnet::encodeSpawn(hostSpawn, wire))
            b.server->broadcast(coopnet::Channel::Control, wire);
    }
    return true;
}

u32 BridgeTakeSpawns(SpawnInfo* out, u32 capacity)
{
    Bridge& b = state();
    if (out == nullptr || capacity == 0)
        return 0;

    u32 written = 0;
    while (written < capacity && !b.pendingSpawns.empty())
    {
        const coopnet::Spawn& spawn = b.pendingSpawns.front();
        SpawnInfo& info = out[written++];
        info.netId = spawn.netId;
        info.size = static_cast<u32>(spawn.data.size());
        std::memcpy(info.data, spawn.data.data(), spawn.data.size());
        b.pendingSpawns.erase(b.pendingSpawns.begin());
    }
    return written;
}

u32 BridgeTakeDespawns(u32* out, u32 capacity)
{
    Bridge& b = state();
    if (out == nullptr || capacity == 0)
        return 0;

    u32 written = 0;
    while (written < capacity && !b.pendingDespawns.empty())
    {
        out[written++] = b.pendingDespawns.front();
        b.pendingDespawns.erase(b.pendingDespawns.begin());
    }
    return written;
}

u32 BridgeLocalSpawnSize()
{
    return static_cast<u32>(state().localSpawn.size());
}

u32 BridgeKnownSpawnCount()
{
    return static_cast<u32>(state().knownSpawns.size());
}

void BridgeSetWorldInfo(pcstr levelName, u64 gameTimeMs)
{
    Bridge& b = state();
    if (levelName == nullptr || levelName[0] == 0)
        return;

    if (b.hasLocalWorld && b.localWorld.levelName != levelName)
        resetLevelState(b);

    b.localWorld.gameTimeMs = gameTimeMs;
    b.localWorld.levelName = levelName;
    b.hasLocalWorld = true;
}

u64 BridgeServerGameTime()
{
    Bridge& b = state();
    if (!b.serverWorldKnown)
        return 0;
    return b.serverClockOffsetMs + b.nowMs;
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