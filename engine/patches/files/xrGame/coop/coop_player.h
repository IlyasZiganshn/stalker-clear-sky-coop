#pragma once

// Game-side glue for coopnet: feeds the local actor transform into the network
// bridge. Everything else (rate limiting, encoding) lives in the bridge.
namespace coop
{
void PublishLocalPlayer();
} // namespace coop