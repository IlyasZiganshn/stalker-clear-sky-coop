#pragma once

#include "xrNetServer/NET_Shared.h"

// M1: transport-only bridge between the engine and coopnet. Sessions live on
// the game thread: console commands start them, CRenderDevice::FrameMove ticks
// them. World replication plugs into the same entry points later.
namespace coop
{
XRNETSERVER_API bool BridgeStartServer(u16 port);
XRNETSERVER_API bool BridgeStartClient(pcstr host, u16 port);
XRNETSERVER_API void BridgeStop();
XRNETSERVER_API void BridgeTick(u32 nowMs);

XRNETSERVER_API bool BridgeActive();
XRNETSERVER_API u32 BridgePeerCount();

// One-line state for console output; the buffer is owned by the bridge.
XRNETSERVER_API pcstr BridgeStatus();
} // namespace coop