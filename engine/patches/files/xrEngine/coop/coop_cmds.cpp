#include "stdafx.h"

#include <cstdlib>

#include "XR_IOConsole.h"
#include "xr_ioc_cmd.h"
#include "IGame_Level.h"
#include "xr_object.h"
#include "xrNetServer/coop/coop_bridge.h"

namespace
{
constexpr u16 kCoopDefaultPort = 27016;

bool parsePort(pcstr text, u16& port)
{
    if (!text || !text[0])
        return false;
    char* end = nullptr;
    const unsigned long value = std::strtoul(text, &end, 10);
    if (end == text || *end != 0 || value == 0 || value > 65535)
        return false;
    port = static_cast<u16>(value);
    return true;
}

class CCC_CoopListen : public IConsole_Command
{
public:
    CCC_CoopListen(pcstr name) : IConsole_Command(name) { bEmptyArgsHandled = true; }

    void Execute(pcstr args) override
    {
        u16 port = kCoopDefaultPort;
        if (args[0] && !parsePort(args, port))
        {
            InvalidSyntax();
            return;
        }
        coop::BridgeStartServer(port);
        Msg("* %s", coop::BridgeStatus());
    }

    void Info(TInfo& I) override { xr_strcpy(I, "[port], default 27016"); }
};

class CCC_CoopConnect : public IConsole_Command
{
public:
    CCC_CoopConnect(pcstr name) : IConsole_Command(name) {}

    void Execute(pcstr args) override
    {
        string256 host{};
        u16 port = kCoopDefaultPort;

        const char* colon = strrchr(args, ':');
        if (colon)
        {
            const std::size_t length = static_cast<std::size_t>(colon - args);
            if (length == 0 || length >= sizeof(host) || !parsePort(colon + 1, port))
            {
                InvalidSyntax();
                return;
            }
            memcpy(host, args, length);
            host[length] = 0;
        }
        else
        {
            if (!args[0] || xr_strlen(args) >= sizeof(host))
            {
                InvalidSyntax();
                return;
            }
            xr_strcpy(host, args);
        }

        coop::BridgeStartClient(host, port);
        Msg("* %s", coop::BridgeStatus());
    }

    void Info(TInfo& I) override { xr_strcpy(I, "host[:port], port default 27016"); }
};

class CCC_CoopStop : public IConsole_Command
{
public:
    CCC_CoopStop(pcstr name) : IConsole_Command(name) { bEmptyArgsHandled = true; }

    void Execute(pcstr) override
    {
        coop::BridgeStop();
        Msg("* %s", coop::BridgeStatus());
    }
};

class CCC_CoopStatus : public IConsole_Command
{
public:
    CCC_CoopStatus(pcstr name) : IConsole_Command(name) { bEmptyArgsHandled = true; }

    void Execute(pcstr) override
    {
        Msg("* %s", coop::BridgeStatus());
        pcstr chat = coop::BridgeLastChat();
        if (chat[0])
            Msg("* coop: last chat %s", chat);

        coop::RemotePlayer remotes[4];
        const u32 count = coop::BridgeRemotePlayers(remotes, 4);
        Msg("* coop: local spawn %u bytes, known spawns %u, remote players %u",
            coop::BridgeLocalSpawnSize(), coop::BridgeKnownSpawnCount(), count);
        for (u32 i = 0; i < count; ++i)
        {
            Msg("* coop: remote %u at %.1f %.1f %.1f, yaw %.2f, body 0x%04x",
                remotes[i].netId, remotes[i].x, remotes[i].y, remotes[i].z,
                remotes[i].yaw, remotes[i].bodyState);
        }

        const u64 serverTime = coop::BridgeServerGameTime();
        if (serverTime != 0)
            Msg("* coop: server game time %llu ms",
                static_cast<unsigned long long>(serverTime));
    }
};

class CCC_CoopDump : public IConsole_Command
{
public:
    CCC_CoopDump(pcstr name) : IConsole_Command(name) { bEmptyArgsHandled = true; }

    void Execute(pcstr) override
    {
        if (g_pGameLevel == nullptr)
        {
            Msg("* coop: no level loaded");
            return;
        }

        // Object ids of two processes are only comparable if both loaded the
        // level the same way; the dump exists to answer that question in game.
        CObjectList& objects = g_pGameLevel->Objects;
        const u32 count = objects.o_count();
        Msg("* coop: %u objects in level", count);
        const u32 limit = count < 256 ? count : 256;
        for (u32 i = 0; i < limit; ++i)
        {
            IGameObject* object = objects.o_get_by_iterator(i);
            if (object == nullptr)
                continue;
            const Fvector& position = object->Position();
            pcstr section = object->cNameSect_str();
            Msg("* coop: obj %u %s at %.1f %.1f %.1f%s", object->ID(),
                section ? section : "?", position.x, position.y, position.z,
                object->Local() ? " local" : "");
        }
    }

    void Info(TInfo& I) override { xr_strcpy(I, "lists level objects and ids"); }
};

class CCC_CoopSay : public IConsole_Command
{
public:
    CCC_CoopSay(pcstr name) : IConsole_Command(name) {}

    void Execute(pcstr args) override
    {
        if (!args[0])
        {
            InvalidSyntax();
            return;
        }
        if (!coop::BridgeSay(args))
            Msg("* coop: cannot send message (not connected or text too long)");
    }

    void Info(TInfo& I) override { xr_strcpy(I, "text"); }
};
} // namespace

void CCC_RegisterCoop()
{
    CMD1(CCC_CoopListen, "coop_listen");
    CMD1(CCC_CoopConnect, "coop_connect");
    CMD1(CCC_CoopStop, "coop_stop");
    CMD1(CCC_CoopStatus, "coop_status");
    CMD1(CCC_CoopSay, "coop_say");
    CMD1(CCC_CoopDump, "coop_dump");
}