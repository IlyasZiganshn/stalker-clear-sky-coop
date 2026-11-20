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
};

// Publishes the local player transform. The bridge rate-limits itself (20 Hz),
// so calling it every frame is fine; no-op without a session.
XRNETSERVER_API void BridgePublishLocalPlayer(float x, float y, float z, float yaw);
// Copies up to capacity fresh remote transforms (stale after 2 s) into out and
// returns how many were written.
XRNETSERVER_API u32 BridgeRemotePlayers(RemotePlayer* out, u32 capacity);

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