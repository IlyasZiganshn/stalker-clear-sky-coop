#pragma once

class CActor;

// Game-side glue for coopnet: publishes the local actor transform and its
// spawn blob, and spawns/removes remote player dolls. Everything about
// encoding, rate limiting and transport lives in the bridge.
namespace coop
{
void PublishLocalPlayer(CActor* actor);
} // namespace coop