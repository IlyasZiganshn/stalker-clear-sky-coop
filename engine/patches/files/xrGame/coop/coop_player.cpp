#include "stdafx.h"

#include "coop_player.h"

#include <cmath>
#include <cstring>

#include "Actor.h"
#include "CharacterPhysicsSupport.h"
#include "Inventory.h"
#include "Level.h"
#include "phmovementcontrol.h"
#include "xrEngine/device.h"
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
// The host simulates dolls from input and broadcasts their state at 20 Hz.
constexpr u32 kDollPublishIntervalMs = 50;

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

// Snaps remote dolls to the last transform heard from the authority. There is
// no interpolation here yet: the bridge already rate-limits samples to 20 Hz.
// Dolls fed by input are simulated locally instead and are skipped.
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
            actor->SetBodyState(players[i].bodyState);
            actor->g_SetAnimation(players[i].bodyState);
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
    SpawnInfo spawns[4];
    const u32 spawnCount = BridgeTakeSpawns(spawns, 4);
    for (u32 i = 0; i < spawnCount; ++i)
        SpawnRemote(spawns[i]);

    u32 gone[8];
    const u32 goneCount = BridgeTakeDespawns(gone, 8);
    for (u32 i = 0; i < goneCount; ++i)
        DespawnRemote(gone[i]);

    PlayerInput inputs[kMaxRemoteEntities];
    const u32 inputCount = BridgeRemoteInputs(inputs, kMaxRemoteEntities);
    for (u32 i = 0; i < inputCount; ++i)
        MoveRemoteDoll(inputs[i], Device.fTimeDelta);

    ApplyRemoteTransforms(inputs, inputCount);
    PublishDollStates(inputs, inputCount);
}

void PublishLocalInput(CActor* actor)
{
    const u32 mstate = actor->MovingState();
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
        actor->Orientation().yaw,
        static_cast<u16>(actor->MovingState() & 0x7FFF));
    PublishLocalInput(actor);

    const shared_str& levelName = Level().name();
    if (levelName.size() > 0)
        BridgeSetWorldInfo(levelName.c_str(), static_cast<u64>(Level().GetGameTime()));
}
} // namespace coop