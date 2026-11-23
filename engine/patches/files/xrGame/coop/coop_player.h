#pragma once

class CActor;
class IGameObject;

// Game-side glue for coopnet: publishes the local actor transform and its
// spawn blob, and spawns/removes remote player dolls. Everything about
// encoding, rate limiting and transport lives in the bridge.
namespace coop
{
void PublishLocalPlayer(CActor* actor);
void OnLevelStop();
void OnRender();
bool IsRemoteDollId(u16 gameId);
u32 FindRemoteNetId(u16 gameId);
bool GetRemotePeerName(u16 gameId, char* out, u32 capacity);
bool IsPlayerOrDoll(IGameObject* obj);
} // namespace coop