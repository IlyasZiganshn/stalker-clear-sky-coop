#include "stdafx.h"

#include "coop_player.h"

#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <vector>

#include "Actor.h"
#include "CharacterPhysicsSupport.h"
#include "Inventory.h"
#include "Level.h"
#include "map_manager.h"
#include "phmovementcontrol.h"
#include "xrEngine/device.h"
#include "xrServer.h"
#include "xrServerEntities/xrMessages.h"
#include "xrServer_Objects_ALife_All.h"
#include "xrEngine/XR_IOConsole.h"
#include "xrNetServer/coop/coop_bridge.h"
#include "ai_space.h"
#include "xrScriptEngine/script_engine.hpp"

namespace coop
{
namespace
{
constexpr u32 kMaxRemoteEntities = 8;
constexpr u16 kFirstRemoteGameId = 0xF000;
constexpr u16 kLastRemoteGameId = 0xFFF0;
// The host simulates dolls from input and broadcasts their state at ~30 Hz.
constexpr u32 kDollPublishIntervalMs = 33;

// net_id <-> local game id of a remote player doll. The host and every client
// allocate their own ids: game ids are only unique inside one process.
struct RemoteEntity
{
    u32 netId = 0;
    u16 gameId = 0xFFFF;
    u16 buttons = 0;
};

RemoteEntity g_remotes[kMaxRemoteEntities];
bool g_localSpawnPublished = false;
u16 g_nextRemoteId = kFirstRemoteGameId;
u32 g_dollPublishMs = 0;
// Level-scoped guards: dolls die with the level, and a session must not leave
// its dolls behind when it stops.
char g_levelName[128] = "";
bool g_hadSession = false;
u32 g_lastTraceMs = 0;
u32 g_driveStartMs = 0;
bool g_driveFinishedLogged = false;

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

// Remote dolls are real level objects, so they must be removed before the
// table is reused: a new level or a stopped session has none of them.
void ResetRemoteEntities()
{
    for (auto& remote : g_remotes)
    {
        if (remote.netId == 0)
            continue;
        if (g_pGameLevel != nullptr)
        {
            Level().MapManager().RemoveMapLocationByObjectID(remote.gameId);
        }
        if (IGameObject* object = Level().Objects.net_Find(remote.gameId))
        {
            object->setDestroy(true);
        }
        remote = RemoteEntity{};
    }
    g_nextRemoteId = kFirstRemoteGameId;
    g_lastTraceMs = 0;
    g_driveStartMs = 0;
    g_driveFinishedLogged = false;
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
    {
        Msg("! coop: local spawn unavailable (no level server)");
        return false;
    }

    CSE_Abstract* entity = Level().Server->ID_to_entity(g_actor->ID());
    if (entity == nullptr)
    {
        Msg("! coop: local spawn unavailable (actor %u has no server entity)", g_actor->ID());
        return false;
    }

    // bLocal = FALSE also strips LOCAL/ASPLAYER: the receiver must not adopt
    // this actor as its own controlled entity.
    NET_Packet packet;
    entity->Spawn_Write(packet, FALSE);
    if (!BridgeSetLocalSpawn(packet.B.data, packet.B.count))
    {
        Msg("! coop: local spawn rejected (%u bytes)", unsigned(packet.B.count));
        return false;
    }
    Msg("* coop: local spawn published (%u bytes)", unsigned(packet.B.count));
    return true;
}

bool SpawnRemote(const SpawnInfo& info)
{
    if (info.netId == 0 || info.size < sizeof(u16) ||
        info.size > sizeof(NET_Packet::B.data))
    {
        Msg("! coop: blob of player %u is unusable (%u bytes)", info.netId,
            unsigned(info.size));
        return false;
    }
    // A repeated spawn (late joiner, retransmit, periodic resend) must not
    // create a second doll — but a doll that died with a reloaded level is
    // replaced, because the blob refers to the new world.
    const u16 knownId = FindRemoteGameId(info.netId);
    if (knownId != 0xFFFF)
    {
        if (Level().Objects.net_Find(knownId) != nullptr)
            return true;
        ForgetRemote(info.netId);
    }

    NET_Packet packet;
    std::memcpy(packet.B.data, info.data, info.size);
    packet.B.count = info.size;
    packet.r_pos = 0;

    u16 header = 0;
    packet.r_begin(header);
    if (header != M_SPAWN)
    {
        Msg("! coop: blob of player %u is not a spawn packet (0x%04x)", info.netId,
            unsigned(header));
        return false;
    }

    shared_str name;
    packet.r_stringZ(name);
    CSE_Abstract* entity = F_entity_Create(name.c_str());
    if (entity == nullptr)
    {
        Msg("! coop: cannot create '%s' for remote player %u", name.c_str(), info.netId);
        return false;
    }

    if (!entity->Spawn_Read(packet) || !entity->match_configuration())
    {
        Msg("! coop: blob of player %u does not read back as '%s'", info.netId,
            name.c_str());
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
    if (g_pGameLevel != nullptr)
    {
        Level().MapManager().AddMapLocation("mp_friend_location", gameId);
    }
    Msg("* coop: remote player %u spawned as game id %u", info.netId, gameId);
    return true;
}

void DespawnRemote(u32 netId)
{
    const u16 gameId = FindRemoteGameId(netId);
    if (gameId == 0xFFFF)
        return;

    if (g_pGameLevel != nullptr)
    {
        Level().MapManager().RemoveMapLocationByObjectID(gameId);
    }
    if (IGameObject* object = Level().Objects.net_Find(gameId))
    {
        object->setDestroy(true);
    }
    ForgetRemote(netId);
    Msg("* coop: remote player %u removed (game id %u)", netId, gameId);
}

// Snaps remote dolls to the transform the bridge interpolated for this frame:
// 20 Hz samples, rendered 100 ms in the past. Dolls fed by input are simulated
// locally instead and are skipped.
void ApplyRemoteTransforms(const PlayerInput* inputs, u32 inputCount)
{
    RemotePlayer players[kMaxRemoteEntities];
    const u32 count = BridgeRemotePlayers(players, kMaxRemoteEntities);
    for (u32 i = 0; i < count; ++i)
    {
        bool simulated = false;
        for (u32 j = 0; j < inputCount; ++j)
            if (inputs[j].netId == players[i].netId)
                simulated = true;
        if (simulated)
            continue;

        const u16 gameId = FindRemoteGameId(players[i].netId);
        if (gameId == 0xFFFF)
            continue;
        IGameObject* object = Level().Objects.net_Find(gameId);
        if (object == nullptr)
            continue;
        // setXYZ rebuilds the rotation and clears the matrix translation, so
        // the position has to be restored after it.
        object->XFORM().setXYZ(0.f, players[i].yaw, 0.f);
        object->Position().set(players[i].x, players[i].y, players[i].z);
        object->spatial_move();

        // The owner still sends its movement bits with the transform, so the
        // animation does not freeze between the rare input samples.
        if (CActor* actor = smart_cast<CActor*>(object))
        {
            actor->Orientation().yaw = players[i].yaw;
            actor->SetBodyState(players[i].bodyState);
            actor->SetWishfulBodyState(players[i].bodyState);
            actor->g_SetAnimation(players[i].bodyState);

            if (actor->character_physics_support() && actor->character_physics_support()->movement())
            {
                actor->character_physics_support()->movement()->SetPosition(actor->Position());
                actor->character_physics_support()->movement()->SetVelocity(0.f, 0.f, 0.f);
            }
        }
    }
}

u32 BuildBodyState(const PlayerInput& input)
{
    u32 mstate = 0;
    if (input.moveY > 0.f)
        mstate |= mcFwd;
    if (input.moveY < 0.f)
        mstate |= mcBack;
    if (input.moveX > 0.f)
        mstate |= mcRStrafe;
    if (input.moveX < 0.f)
        mstate |= mcLStrafe;
    if (input.buttons & kInputButtonCrouch)
        mstate |= mcCrouch;
    if (input.buttons & kInputButtonJump)
        mstate |= mcJump;

    // g_cl_CheckControls clears sprint without forward or strafe input and
    // while crouching.
    const bool moving = (mstate & (mcFwd | mcLStrafe | mcRStrafe)) != 0;
    const bool crouch = (mstate & mcCrouch) != 0;
    if ((input.buttons & kInputButtonSprint) && moving && !crouch)
        mstate |= mcSprint;
    return mstate;
}

// Same speed rules as g_cl_CheckControls, minus backpack and limp modifiers.
// The owner is treated as running (the engine default); the walk key and zoom
// are not replicated yet.
Fvector BuildAccel(const CActor& actor, u32 mstate, const PlayerInput& input)
{
    Fvector accel;
    accel.set(input.moveX, 0.f, input.moveY);
    const float length = accel.magnitude();
    if (length < EPS)
        return Fvector{0.f, 0.f, 0.f};
    accel.mul(1.f / length);

    const bool crouch = (mstate & mcCrouch) != 0;
    const bool sprint = (mstate & mcSprint) != 0;
    const bool back = (mstate & mcBack) != 0;
    const bool strafe = (mstate & (mcLStrafe | mcRStrafe)) != 0;

    float speed = actor.m_fWalkAccel;
    speed *= back ? actor.m_fRunBackFactor : actor.m_fRunFactor;
    if (crouch)
        speed *= actor.m_fCrouchFactor;
    if (sprint)
        speed *= actor.m_fSprintFactor;
    if (strafe && !crouch)
        speed *= actor.m_fRun_StrafeFactor;
    accel.mul(speed);

    // g_cl_CheckControls turns the local accel into a world direction with the
    // body yaw; reuse the same transform so signs always match the engine.
    Fmatrix orient;
    orient.rotateY(-input.yaw);
    orient.transform_dir(accel);
    return accel;
}

void RememberButtons(u32 netId, u16 buttons)
{
    for (auto& remote : g_remotes)
        if (remote.netId == netId)
            remote.buttons = buttons;
}

u16 PreviousButtons(u32 netId)
{
    for (const auto& remote : g_remotes)
        if (remote.netId == netId)
            return remote.buttons;
    return 0;
}

// Drives a doll from the owner's input with the engine character controller:
// the host owns the physics, the owner only sends intent.
void MoveRemoteDoll(const PlayerInput& input, float dt)
{
    const u16 gameId = FindRemoteGameId(input.netId);
    if (gameId == 0xFFFF)
        return;
    CActor* actor = smart_cast<CActor*>(Level().Objects.net_Find(gameId));
    if (actor == nullptr || !actor->g_Alive())
        return;

    CPHMovementControl* movement = actor->character_physics_support()->movement();
    if (movement == nullptr || !movement->CharacterExist())
        return;

    const u32 mstate = BuildBodyState(input);
    Fvector accel = BuildAccel(*actor, mstate, input);

    float jump = 0.f;
    const bool jumpPressed = (input.buttons & kInputButtonJump) != 0 &&
        (PreviousButtons(input.netId) & kInputButtonJump) == 0;
    const CPHMovementControl::EEnvironment environment = movement->Environment();
    if (jumpPressed &&
        (environment == CPHMovementControl::peOnGround ||
            environment == CPHMovementControl::peAtWall))
        jump = actor->m_fJumpSpeed;
    RememberButtons(input.netId, input.buttons);

    actor->SetBodyState(mstate);
    actor->SetWishfulBodyState(mstate);

    // Camera direction for the character controller (ladders, elevators):
    // yaw maps to forward as rotateY(-yaw), pitch is positive downwards.
    const float cp = std::cos(input.pitch);
    const float sp = std::sin(input.pitch);
    Fvector camDir;
    camDir.set(-std::sin(input.yaw) * cp, -sp, std::cos(input.yaw) * cp);
    movement->Calculate(accel, camDir, 0.f, jump, dt, false);

    Fvector position;
    movement->GetPosition(position);
    actor->XFORM().setXYZ(0.f, input.yaw, 0.f);
    actor->Position().set(position);
    actor->spatial_move();

    actor->Orientation().yaw = input.yaw;
    actor->Orientation().pitch = input.pitch;
    actor->g_SetAnimation(mstate);
}

// The owning client keeps its own prediction; everyone else must see the
// authority's doll, so the host broadcasts the simulated transform.
void PublishDollStates(const PlayerInput* inputs, u32 inputCount)
{
    if (inputCount == 0 || Device.dwTimeGlobal - g_dollPublishMs < kDollPublishIntervalMs)
        return;
    g_dollPublishMs = Device.dwTimeGlobal;

    for (u32 i = 0; i < inputCount; ++i)
    {
        const u16 gameId = FindRemoteGameId(inputs[i].netId);
        if (gameId == 0xFFFF)
            continue;
        CActor* actor = smart_cast<CActor*>(Level().Objects.net_Find(gameId));
        if (actor == nullptr)
            continue;
        const Fvector& position = actor->Position();
        BridgePublishRemoteState(inputs[i].netId, position.x, position.y,
            position.z, inputs[i].yaw,
            static_cast<u16>(actor->MovingState() & 0x7FFF));
    }
}

void PollRemoteEntities()
{
    // Despawns go first: a replaced spawn is queued as despawn + spawn in the
    // same tick, and the old doll must be gone before the new one is built.
    u32 gone[8];
    const u32 goneCount = BridgeTakeDespawns(gone, 8);
    for (u32 i = 0; i < goneCount; ++i)
        DespawnRemote(gone[i]);

    SpawnInfo spawns[4];
    const u32 spawnCount = BridgeTakeSpawns(spawns, 4);
    for (u32 i = 0; i < spawnCount; ++i)
        SpawnRemote(spawns[i]);

    PlayerInput inputs[kMaxRemoteEntities];
    const u32 inputCount = BridgeRemoteInputs(inputs, kMaxRemoteEntities);
    for (u32 i = 0; i < inputCount; ++i)
        MoveRemoteDoll(inputs[i], Device.fTimeDelta);

    ApplyRemoteTransforms(inputs, inputCount);
    PublishDollStates(inputs, inputCount);
}

struct DrivePhase
{
    u32 durationMs = 0;
    u32 mstate = 0;
};

struct DrivePlan
{
    u32 delayMs = 0;
    std::vector<DrivePhase> phases;
    u32 totalDurationMs = 0;
    bool valid = false;
};

DrivePlan ParseDriveScript(pcstr script)
{
    DrivePlan plan;
    if (script == nullptr || script[0] == 0)
        return plan;

    while (*script == ' ' || *script == '\t')
        ++script;

    char* end = nullptr;
    const unsigned long delay = std::strtoul(script, &end, 10);
    if (end == script)
        return plan;
    plan.delayMs = static_cast<u32>(delay);

    while (*end == ' ' || *end == '\t')
        ++end;

    pcstr cursor = end;
    plan.totalDurationMs = plan.delayMs;

    while (cursor != nullptr && *cursor != 0)
    {
        pcstr next = strchr(cursor, ',');
        char token[64] = "";
        const size_t len = next ? static_cast<size_t>(next - cursor) : xr_strlen(cursor);
        if (len == 0 || len >= sizeof(token))
            break;
        std::memcpy(token, cursor, len);
        token[len] = 0;

        char* durEnd = nullptr;
        const unsigned long dur = std::strtoul(token, &durEnd, 10);
        if (durEnd != token && dur > 0)
        {
            DrivePhase phase;
            phase.durationMs = static_cast<u32>(dur);
            for (const char* f = durEnd; *f != 0; ++f)
            {
                switch (*f)
                {
                case 'f': phase.mstate |= mcFwd; break;
                case 'b': phase.mstate |= mcBack; break;
                case 'l': phase.mstate |= mcLStrafe; break;
                case 'r': phase.mstate |= mcRStrafe; break;
                case 'c': phase.mstate |= mcCrouch; break;
                case 'a': phase.mstate |= mcAccel; break;
                case 't': phase.mstate |= mcSprint; break;
                case 'j': phase.mstate |= mcJump; break;
                default: break;
                }
            }
            plan.phases.push_back(phase);
            plan.totalDurationMs += phase.durationMs;
        }

        cursor = next ? next + 1 : nullptr;
    }

    plan.valid = !plan.phases.empty();
    return plan;
}

char g_lastDriveScript[256] = "";
DrivePlan g_currentPlan;

u32 EvaluateDebugDrive(u32 nowMs)
{
    pcstr script = BridgeDebugDrive();
    if (script == nullptr || script[0] == 0)
    {
        g_driveStartMs = 0;
        g_driveFinishedLogged = false;
        g_lastDriveScript[0] = 0;
        g_currentPlan = DrivePlan{};
        return 0;
    }

    if (xr_strcmp(g_lastDriveScript, script) != 0)
    {
        std::snprintf(g_lastDriveScript, sizeof(g_lastDriveScript), "%s", script);
        g_currentPlan = ParseDriveScript(script);
        g_driveStartMs = 0;
        g_driveFinishedLogged = false;
    }

    if (!g_currentPlan.valid)
        return 0;

    // Wait until peer has joined and level is active
    if (BridgePeerCount() == 0 || !g_localSpawnPublished)
    {
        g_driveStartMs = 0;
        return 0;
    }

    if (g_driveStartMs == 0)
    {
        g_driveStartMs = nowMs;
        Msg("* coop: debug drive started (delay %u ms, %u phase(s), total %u ms)",
            g_currentPlan.delayMs, static_cast<u32>(g_currentPlan.phases.size()),
            g_currentPlan.totalDurationMs);
    }

    const u32 elapsed = nowMs - g_driveStartMs;
    if (elapsed < g_currentPlan.delayMs)
        return 0;

    u32 phaseStart = g_currentPlan.delayMs;
    for (const auto& phase : g_currentPlan.phases)
    {
        const u32 phaseEnd = phaseStart + phase.durationMs;
        if (elapsed >= phaseStart && elapsed < phaseEnd)
            return phase.mstate;
        phaseStart = phaseEnd;
    }

    if (!g_driveFinishedLogged)
    {
        Msg("* coop: debug drive finished after %u ms", elapsed);
        g_driveFinishedLogged = true;
    }
    return 0;
}

void TraceDollStates(CActor* actor, u32 nowMs)
{
    const u32 traceInterval = BridgeDebugTraceMs();
    if (traceInterval == 0 || nowMs - g_lastTraceMs < traceInterval)
        return;
    g_lastTraceMs = nowMs;

    if (actor != nullptr)
    {
        const Fvector& pos = actor->Position();
        Msg("* coop: trace actor at %.2f %.2f %.2f, yaw %.2f, mstate 0x%04x",
            pos.x, pos.y, pos.z, actor->Orientation().yaw, actor->MovingState());
    }

    for (const auto& remote : g_remotes)
    {
        if (remote.netId == 0 || remote.gameId == 0xFFFF)
            continue;
        IGameObject* obj = Level().Objects.net_Find(remote.gameId);
        if (obj == nullptr)
            continue;
        CActor* remoteActor = smart_cast<CActor*>(obj);
        const Fvector& pos = obj->Position();
        const u32 mstate = remoteActor ? remoteActor->MovingState() : 0;
        Msg("* coop: trace doll %u (game id %u) at %.2f %.2f %.2f, yaw %.2f, mstate 0x%04x",
            remote.netId, remote.gameId, pos.x, pos.y, pos.z, obj->Orientation().yaw, mstate);
    }
}

void DumpLevelObjects(u32 limit)
{
    CObjectList& objects = Level().Objects;
    const u32 count = objects.o_count();
    std::vector<IGameObject*> found;
    found.reserve(count);
    for (u32 i = 0; i < count; ++i)
        if (IGameObject* object = objects.o_get_by_iterator(i))
            found.push_back(object);

    if (limit > found.size())
        limit = static_cast<u32>(found.size());

    std::sort(found.begin(), found.end(),
        [](const IGameObject* a, const IGameObject* b) { return a->ID() < b->ID(); });

    Msg("* coop: %u objects in level, dumping %u", count, limit);
    for (u32 i = 0; i < limit; ++i)
    {
        IGameObject* object = found[i];
        const Fvector& position = object->Position();
        pcstr section = object->cNameSect_str();
        Msg("* coop: obj %u %s at %.1f %.1f %.1f%s", object->ID(),
            section ? section : "?", position.x, position.y, position.z,
            object->Local() ? " local" : "");
    }
}

void PublishLocalInput(CActor* actor, u32 driveMstate)
{
    const u32 mstate = actor->MovingState() | driveMstate;
    float moveX = 0.f;
    float moveY = 0.f;
    if (mstate & mcFwd)
        moveY += 1.f;
    if (mstate & mcBack)
        moveY -= 1.f;
    if (mstate & mcRStrafe)
        moveX += 1.f;
    if (mstate & mcLStrafe)
        moveX -= 1.f;

    u16 buttons = 0;
    if (mstate & mcJump)
        buttons |= kInputButtonJump;
    if (mstate & mcCrouch)
        buttons |= kInputButtonCrouch;
    if (mstate & mcSprint)
        buttons |= kInputButtonSprint;

    const SRotation orientation = actor->Orientation();
    BridgePublishLocalInput(moveX, moveY, orientation.yaw, orientation.pitch,
        buttons, static_cast<u8>(actor->inventory().GetActiveSlot()));
}
} // namespace

void PublishLocalPlayer(CActor* actor)
{
    if (!BridgeActive())
    {
        g_localSpawnPublished = false;
        if (g_hadSession)
        {
            g_hadSession = false;
            ResetRemoteEntities();
        }
        return;
    }
    // Remote dolls also run CActor::UpdateCL; only the local actor publishes.
    if (actor == nullptr || actor != g_actor || actor->getDestroy())
        return;

    g_hadSession = true;

    const shared_str& levelName = Level().name();
    if (levelName.size() > 0 && xr_strcmp(g_levelName, levelName.c_str()) != 0)
    {
        std::snprintf(g_levelName, sizeof(g_levelName), "%s", levelName.c_str());
        g_localSpawnPublished = false;
        ResetRemoteEntities();
    }

    // The bridge must drop the previous level's spawns before this frame feeds
    // any of them back through BridgeTakeSpawns.
    if (levelName.size() > 0)
        BridgeSetWorldInfo(levelName.c_str(), static_cast<u64>(Level().GetGameTime()));

    char targetLevel[64] = "";
    if (BridgeTakeLevelChange(targetLevel, sizeof(targetLevel)))
    {
        Msg("* coop: host moved to level '%s', synchronizing transition...", targetLevel);
        string256 command;
        xr_sprintf(command, sizeof(command), "jump_to_level %s", targetLevel);
        Console->Execute(command);
        return;
    }

    if (!g_localSpawnPublished)
        g_localSpawnPublished = PublishLocalSpawn();

    const u32 autoDump = BridgeTakeAutoDump();
    if (autoDump > 0)
        DumpLevelObjects(autoDump);

    const u32 driveMstate = EvaluateDebugDrive(Device.dwTimeGlobal);
    if (driveMstate != 0)
    {
        actor->SetWishfulBodyState(driveMstate);
    }

    PollRemoteEntities();

    TraceDollStates(actor, Device.dwTimeGlobal);

    const Fvector& position = actor->Position();
    BridgePublishLocalPlayer(position.x, position.y, position.z,
        actor->Orientation().yaw,
        static_cast<u16>(actor->MovingState() & 0x7FFF));
    PublishLocalInput(actor, driveMstate);

    // Invoke Lua coop campaign hooks
    if (GEnv.ScriptEngine != nullptr)
    {
        luabind::functor<void> updateFn;
        if (GEnv.ScriptEngine->functor("coop_main.update", updateFn))
        {
            try {
                updateFn(Device.fTimeDelta);
            } catch (...) {}
        }
        luabind::functor<void> roleFn;
        if (GEnv.ScriptEngine->functor("coop_main.set_role", roleFn))
        {
            try {
                roleFn(BridgeIsServer());
            } catch (...) {}
        }
    }
}

void OnLevelStop()
{
    g_localSpawnPublished = false;
    g_levelName[0] = 0;
    ResetRemoteEntities();
}

bool IsRemoteDollId(u16 gameId)
{
    return gameId >= kFirstRemoteGameId && gameId <= kLastRemoteGameId;
}

u32 FindRemoteNetId(u16 gameId)
{
    for (const auto& remote : g_remotes)
        if (remote.gameId == gameId)
            return remote.netId;
    return 0;
}

bool GetRemotePeerName(u16 gameId, char* out, u32 capacity)
{
    const u32 netId = FindRemoteNetId(gameId);
    if (netId == 0)
        return false;
    return BridgePeerName(netId, out, capacity);
}

bool IsPlayerOrDoll(IGameObject* obj)
{
    if (obj == nullptr)
        return false;
    if (g_pGameLevel != nullptr && obj == Level().CurrentEntity())
        return true;
    return IsRemoteDollId(obj->ID());
}

void OnRender()
{
    if (g_pGameLevel == nullptr)
        return;

    for (const auto& remote : g_remotes)
    {
        if (remote.netId == 0 || remote.gameId == 0xFFFF)
            continue;

        IGameObject* object = Level().Objects.net_Find(remote.gameId);
        if (object == nullptr)
            continue;

        CActor* doll = smart_cast<CActor*>(object);
        if (doll == nullptr || !doll->g_Alive())
            continue;

        char peerName[64] = "Partner";
        if (!BridgePeerName(remote.netId, peerName, sizeof(peerName)) || peerName[0] == 0)
        {
            xr_sprintf(peerName, "Partner");
        }

        // 1. Draw Name above head
        Fvector dpos{ 0.f, 0.40f, 0.f };
        float dup = 0.f;
        // Cyan-green name color (0xEE32FF96)
        doll->RenderText(peerName, dpos, &dup, color_rgba(50, 255, 150, 238));

        // 2. Health percentage & status
        float health = doll->GetfHealth();
        if (health < 0.f) health = 0.f;
        if (health > 1.f) health = 1.f;
        int hpPercent = static_cast<int>(health * 100.f + 0.5f);

        char hpStr[64];
        u32 hpColor;
        if (hpPercent > 50)
        {
            hpColor = color_rgba(80, 240, 80, 230); // Green
            xr_sprintf(hpStr, "[HP: %d%%]", hpPercent);
        }
        else if (hpPercent > 20)
        {
            hpColor = color_rgba(255, 200, 40, 230); // Orange
            xr_sprintf(hpStr, "[HP: %d%%]", hpPercent);
        }
        else if (hpPercent > 0)
        {
            hpColor = color_rgba(255, 50, 50, 240); // Red
            xr_sprintf(hpStr, "[HP: %d%% - WOUNDED]", hpPercent);
        }
        else
        {
            hpColor = color_rgba(255, 30, 30, 255);
            xr_sprintf(hpStr, "[DOWNED / РАНЕН]");
        }

        dpos.y += 0.22f;
        doll->RenderText(hpStr, dpos, &dup, hpColor);
    }
}
} // namespace coop
