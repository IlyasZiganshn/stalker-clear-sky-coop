#include "stdafx.h"

#include "coop_player.h"

#include <cstring>

#include "Actor.h"
#include "Level.h"
#include "xrServer.h"
#include "xrServerEntities/xrMessages.h"
#include "xrServer_Objects_ALife_All.h"
#include "xrNetServer/coop/coop_bridge.h"

namespace coop
{
namespace
{
constexpr u32 kMaxRemoteEntities = 8;
constexpr u16 kFirstRemoteGameId = 0xF000;
constexpr u16 kLastRemoteGameId = 0xFFF0;

// net_id <-> local game id of a remote player doll. The host and every client
// allocate their own ids: game ids are only unique inside one process.
struct RemoteEntity
{
    u32 netId = 0;
    u16 gameId = 0xFFFF;
};

RemoteEntity g_remotes[kMaxRemoteEntities];
bool g_localSpawnPublished = false;
u16 g_nextRemoteId = kFirstRemoteGameId;

u16 FindRemoteGameId(u32 netId)
{
    for (const auto& remote : g_remotes)
        if (remote.netId == netId)
            return remote.gameId;
    return 0xFFFF;
}

void RememberRemote(u32 netId, u16 gameId)
{
    for (auto& remote : g_remotes)
    {
        if (remote.netId == 0 || remote.netId == netId)
        {
            remote.netId = netId;
            remote.gameId = gameId;
            return;
        }
    }
    g_remotes[0].netId = netId;
    g_remotes[0].gameId = gameId;
}

void ForgetRemote(u32 netId)
{
    for (auto& remote : g_remotes)
    {
        if (remote.netId == netId)
        {
            remote = RemoteEntity{};
            return;
        }
    }
}

// Remote dolls live in a high id range that normal level spawns do not use;
// the freed id is reused after the player leaves.
u16 AllocateRemoteGameId()
{
    constexpr u32 kRange = kLastRemoteGameId - kFirstRemoteGameId + 1;
    u16 id = g_nextRemoteId;
    for (u32 attempt = 0; attempt < kRange; ++attempt)
    {
        if (Level().Objects.net_Find(id) == nullptr)
        {
            g_nextRemoteId = id >= kLastRemoteGameId ? kFirstRemoteGameId : u16(id + 1);
            return id;
        }
        id = id >= kLastRemoteGameId ? kFirstRemoteGameId : u16(id + 1);
    }
    return 0xFFFF;
}

bool PublishLocalSpawn()
{
    if (Level().Server == nullptr)
        return false;

    CSE_Abstract* entity = Level().Server->ID_to_entity(g_actor->ID());
    if (entity == nullptr)
        return false;

    // bLocal = FALSE also strips LOCAL/ASPLAYER: the receiver must not adopt
    // this actor as its own controlled entity.
    NET_Packet packet;
    entity->Spawn_Write(packet, FALSE);
    return BridgeSetLocalSpawn(packet.B.data, packet.B.count);
}

bool SpawnRemote(const SpawnInfo& info)
{
    if (info.netId == 0 || info.size < sizeof(u16) ||
        info.size > sizeof(NET_Packet::B.data))
        return false;
    // A repeated spawn (late joiner, retransmit) must not create a second doll.
    if (FindRemoteGameId(info.netId) != 0xFFFF)
        return true;

    NET_Packet packet;
    std::memcpy(packet.B.data, info.data, info.size);
    packet.B.count = info.size;
    packet.r_pos = 0;

    u16 header = 0;
    packet.r_begin(header);
    if (header != M_SPAWN)
        return false;

    shared_str name;
    packet.r_stringZ(name);
    CSE_Abstract* entity = F_entity_Create(name.c_str());
    if (entity == nullptr)
        return false;

    if (!entity->Spawn_Read(packet) || !entity->match_configuration())
    {
        F_entity_Destroy(entity);
        return false;
    }

    const u16 gameId = AllocateRemoteGameId();
    if (gameId == 0xFFFF)
    {
        F_entity_Destroy(entity);
        Msg("! coop: no free game id for remote player %u", info.netId);
        return false;
    }

    entity->ID = gameId;
    entity->s_flags.set(M_SPAWN_OBJECT_LOCAL, FALSE);
    entity->s_flags.set(M_SPAWN_OBJECT_ASPLAYER, FALSE);

    Level().g_sv_Spawn(entity);
    F_entity_Destroy(entity);

    IGameObject* object = Level().Objects.net_Find(gameId);
    if (object == nullptr)
    {
        Msg("! coop: spawn of remote player %u failed", info.netId);
        return false;
    }

    // g_sv_Spawn marks everything local in single player; a remote doll is
    // simulated by the authority, so it must be remote and stay awake.
    object->setLocal(false);
    object->setEnabled(true);

    RememberRemote(info.netId, gameId);
    Msg("* coop: remote player %u spawned as game id %u", info.netId, gameId);
    return true;
}

void DespawnRemote(u32 netId)
{
    const u16 gameId = FindRemoteGameId(netId);
    if (gameId == 0xFFFF)
        return;

    if (IGameObject* object = Level().Objects.net_Find(gameId))
    {
        object->net_Destroy();
        Level().Objects.Destroy(object);
    }
    ForgetRemote(netId);
    Msg("* coop: remote player %u removed (game id %u)", netId, gameId);
}

void PollRemoteEntities()
{
    SpawnInfo spawns[4];
    const u32 spawnCount = BridgeTakeSpawns(spawns, 4);
    for (u32 i = 0; i < spawnCount; ++i)
        SpawnRemote(spawns[i]);

    u32 gone[8];
    const u32 goneCount = BridgeTakeDespawns(gone, 8);
    for (u32 i = 0; i < goneCount; ++i)
        DespawnRemote(gone[i]);
}
} // namespace

void PublishLocalPlayer(CActor* actor)
{
    if (!BridgeActive())
    {
        g_localSpawnPublished = false;
        return;
    }
    // Remote dolls also run CActor::UpdateCL; only the local actor publishes.
    if (actor == nullptr || actor != g_actor || actor->getDestroy())
        return;

    if (!g_localSpawnPublished)
        g_localSpawnPublished = PublishLocalSpawn();

    PollRemoteEntities();

    const Fvector& position = actor->Position();
    BridgePublishLocalPlayer(position.x, position.y, position.z,
        actor->Orientation().yaw);

    const shared_str& levelName = Level().name();
    if (levelName.size() > 0)
        BridgeSetWorldInfo(levelName.c_str(), static_cast<u64>(Level().GetGameTime()));
}
} // namespace coop