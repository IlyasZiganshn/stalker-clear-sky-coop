#pragma once

#include "xrNetServer/NET_Shared.h"

// Bridge between the engine and coopnet. Sessions live on the game thread:
// console commands start them, CRenderDevice::FrameMove ticks them. Application
// messages (chat) already flow; world replication plugs into the same entry
// points later.
namespace coop
{
XRNETSERVER_API bool BridgeStartServer(u16 port);
XRNETSERVER_API bool BridgeStartClient(pcstr host, u16 port);
XRNETSERVER_API void BridgeStop();
XRNETSERVER_API void BridgeTick(u32 nowMs);

XRNETSERVER_API bool BridgeActive();
XRNETSERVER_API u32 BridgePeerCount();

// Sends a chat line: a client sends it to the server, the server broadcasts it
// to every joined peer. False when there is no session or the text is too long.
XRNETSERVER_API bool BridgeSay(pcstr text);
// Last chat line seen in this process, empty when there was none.
XRNETSERVER_API pcstr BridgeLastChat();

// One transform of a remote player, as last heard from the authority.
struct RemotePlayer
{
    u32 netId = 0;
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;
    float yaw = 0.f;
    u16 bodyState = 0; // engine movement bits (mc*), for the doll animation
};

// Publishes the local player transform and its movement bits. The bridge
// rate-limits itself (20 Hz), so calling it every frame is fine; no-op without
// a session.
XRNETSERVER_API void BridgePublishLocalPlayer(float x, float y, float z, float yaw,
    u16 bodyState);
// Copies up to capacity fresh remote transforms (stale after 2 s) into out and
// returns how many were written.
XRNETSERVER_API u32 BridgeRemotePlayers(RemotePlayer* out, u32 capacity);

// Buttons carried by PlayerInput::buttons; values match coopnet::InputButton.
enum InputButtons : u16
{
    kInputButtonFire = 1u << 0,
    kInputButtonAim = 1u << 1,
    kInputButtonJump = 1u << 2,
    kInputButtonCrouch = 1u << 3,
    kInputButtonSprint = 1u << 4,
    kInputButtonUse = 1u << 5,
    kInputButtonReload = 1u << 6,
};

// One input sample of a remote player: what its owner held this frame. The
// host turns it into movement of the doll; clients only see transforms.
struct PlayerInput
{
    u32 netId = 0;
    float moveX = 0.f;   // [-1, 1], strafe right positive
    float moveY = 0.f;   // [-1, 1], forward positive
    float yaw = 0.f;     // radians
    float pitch = 0.f;   // radians
    u16 buttons = 0;
    u8 weaponSlot = 0;
};

// Publishes the local player input (client -> host), throttled to 60 Hz.
// No-op on the host: its own actor is driven by local input already.
XRNETSERVER_API void BridgePublishLocalInput(float moveX, float moveY, float yaw,
    float pitch, u16 buttons, u8 weaponSlot);
// Copies the latest input of every remote player (stale after 2 s).
XRNETSERVER_API u32 BridgeRemoteInputs(PlayerInput* out, u32 capacity);

// Host only: broadcasts the transform and movement bits of a doll simulated
// from input, so every peer sees the authority's position instead of the
// owner's prediction. Call it at snapshot rate (20 Hz); the bridge does not
// throttle per doll.
XRNETSERVER_API void BridgePublishRemoteState(u32 netId, float x, float y,
    float z, float yaw, u16 bodyState);

// One serialized entity spawn (CSE_Abstract bytes) tagged with its owner.
// The bridge never parses the blob; the game side does.
constexpr u32 kCoopSpawnBlobMax = 4096;

struct SpawnInfo
{
    u32 netId = 0;
    u32 size = 0;
    std::uint8_t data[kCoopSpawnBlobMax] = {};
};

// Hands the local actor spawn to the bridge: a client sends it to the host on
// join, a host sends it to every peer (including later joiners). Safe to call
// every frame; false means the blob is empty or too large.
XRNETSERVER_API bool BridgeSetLocalSpawn(const void* data, u32 size);
// Copies spawns received from peers into out and returns how many were written.
// Each net_id is reported once until it leaves and joins again.
XRNETSERVER_API u32 BridgeTakeSpawns(SpawnInfo* out, u32 capacity);
// Copies net_ids of players that left; the game side destroys its copies.
XRNETSERVER_API u32 BridgeTakeDespawns(u32* out, u32 capacity);

// Diagnostics: bytes of the local spawn blob and number of peer spawns the
// bridge knows (late joiners get all of them).
XRNETSERVER_API u32 BridgeLocalSpawnSize();
XRNETSERVER_API u32 BridgeKnownSpawnCount();

// Tells the bridge which level and game clock the local process is on. Call it
// every frame; outgoing world info is throttled. A client whose level differs
// from the server's is disconnected with a message in BridgeStatus().
XRNETSERVER_API void BridgeSetWorldInfo(pcstr levelName, u64 gameTimeMs);
// Server game clock as this client estimates it (last sample + local time);
// 0 when the client has not heard from the server yet.
XRNETSERVER_API u64 BridgeServerGameTime();

// One-line state for console output; the buffer is owned by the bridge.
XRNETSERVER_API pcstr BridgeStatus();
} // namespace coop