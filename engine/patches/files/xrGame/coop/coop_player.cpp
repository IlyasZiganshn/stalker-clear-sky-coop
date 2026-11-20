#include "stdafx.h"

#include "coop_player.h"

#include "Actor.h"
#include "Level.h"
#include "xrNetServer/coop/coop_bridge.h"

namespace coop
{
void PublishLocalPlayer()
{
    if (g_actor == nullptr || g_actor->getDestroy() || !BridgeActive())
        return;

    const Fvector& position = g_actor->Position();
    BridgePublishLocalPlayer(position.x, position.y, position.z,
        g_actor->Orientation().yaw);

    const shared_str& levelName = Level().name();
    if (levelName.size() > 0)
        BridgeSetWorldInfo(levelName.c_str(), static_cast<u64>(Level().GetGameTime()));
}
} // namespace coop