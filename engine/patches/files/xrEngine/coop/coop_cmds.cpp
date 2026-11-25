#include "stdafx.h"

#include <algorithm>
#include <cstdlib>
#include <vector>

#include "XR_IOConsole.h"
#include "xr_ioc_cmd.h"
#include "IGame_Level.h"
#include "xr_object.h"
#include "Render.h"
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
        // "host[:port] [name]": everything after the first word is the name.
        string256 address{};
        const char* separator = strpbrk(args, " \t");
        const std::size_t length =
            separator ? static_cast<std::size_t>(separator - args) : xr_strlen(args);
        if (length == 0 || length >= sizeof(address))
        {
            InvalidSyntax();
            return;
        }
        memcpy(address, args, length);
        address[length] = 0;

        pcstr name = separator;
        while (name && (*name == ' ' || *name == '\t'))
            ++name;
        if (name && !name[0])
            name = nullptr;

        u16 port = kCoopDefaultPort;
        const char* colon = strrchr(address, ':');
        if (colon)
        {
            const std::size_t hostLength = static_cast<std::size_t>(colon - address);
            if (hostLength == 0 || !parsePort(colon + 1, port))
            {
                InvalidSyntax();
                return;
            }
            address[hostLength] = 0;
        }

        coop::BridgeSetPlayerName(name);
        coop::BridgeStartClient(address, port);
        Msg("* %s", coop::BridgeStatus());
    }

    void Info(TInfo& I) override { xr_strcpy(I, "host[:port] [name], port default 27016"); }
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
            char name[64] = "";
            coop::BridgePeerName(remotes[i].netId, name, sizeof(name));
            Msg("* coop: remote %u%s%s at %.1f %.1f %.1f, yaw %.2f, body 0x%04x",
                remotes[i].netId, name[0] ? " " : "", name,
                remotes[i].x, remotes[i].y, remotes[i].z,
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

    void Execute(pcstr args) override
    {
        if (g_pGameLevel == nullptr)
        {
            u32 limit = 100;
            if (args[0])
            {
                char* end = nullptr;
                const unsigned long val = std::strtoul(args, &end, 10);
                if (end != args && *end == 0 && val > 0)
                    limit = static_cast<u32>(val);
            }
            coop::BridgeSetAutoDump(limit);
            Msg("* coop: dump queued for level load (limit %u)", limit);
            return;
        }

        // Object ids of two processes are only comparable if both loaded the
        // level the same way; the dump exists to answer that question in game.
        CObjectList& objects = g_pGameLevel->Objects;
        const u32 count = objects.o_count();
        std::vector<IGameObject*> found;
        found.reserve(count);
        for (u32 i = 0; i < count; ++i)
            if (IGameObject* object = objects.o_get_by_iterator(i))
                found.push_back(object);

        u32 limit = static_cast<u32>(found.size());
        if (args[0])
        {
            char* end = nullptr;
            const unsigned long value = std::strtoul(args, &end, 10);
            if (end == args || *end != 0 || value == 0)
            {
                InvalidSyntax();
                return;
            }
            if (value < limit)
                limit = static_cast<u32>(value);
        }

        // Enumeration order may differ between processes, so sort by id: two
        // logs can then be compared line by line.
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

    void Info(TInfo& I) override { xr_strcpy(I, "[limit], objects sorted by id"); }
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

class CCC_CoopDrive : public IConsole_Command
{
public:
    CCC_CoopDrive(pcstr name) : IConsole_Command(name) { bEmptyArgsHandled = true; }

    void Execute(pcstr args) override
    {
        coop::BridgeSetDebugDrive(args);
        Msg("* coop: debug drive '%s'", coop::BridgeDebugDrive());
    }

    void Info(TInfo& I) override { xr_strcpy(I, "<delayMs> <durationMs><flags>[,...]"); }
};

class CCC_CoopTrace : public IConsole_Command
{
public:
    CCC_CoopTrace(pcstr name) : IConsole_Command(name) { bEmptyArgsHandled = true; }

    void Execute(pcstr args) override
    {
        u32 intervalMs = 0;
        if (args[0])
        {
            char* end = nullptr;
            const unsigned long val = std::strtoul(args, &end, 10);
            if (end != args && *end == 0)
                intervalMs = static_cast<u32>(val);
        }
        coop::BridgeSetDebugTraceMs(intervalMs);
        Msg("* coop: debug trace interval %u ms", intervalMs);
    }

    void Info(TInfo& I) override { xr_strcpy(I, "[intervalMs], 0 turns it off"); }
};

class CCC_CoopScreenshot : public IConsole_Command
{
public:
    CCC_CoopScreenshot(pcstr name) : IConsole_Command(name) { bEmptyArgsHandled = true; }

    void Execute(pcstr args) override
    {
        if (GEnv.Render != nullptr)
        {
            GEnv.Render->Screenshot();
            Msg("* coop: in-game screenshot captured");
        }
    }

    void Info(TInfo& I) override { xr_strcpy(I, "captures in-game screenshot to $screenshots$"); }
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
    CMD1(CCC_CoopDrive, "coop_drive");
    CMD1(CCC_CoopTrace, "coop_trace");
    CMD1(CCC_CoopScreenshot, "screenshot");
    CMD1(CCC_CoopScreenshot, "coop_screenshot");
}

namespace coop
{
void PollCoopCmdFile()
{
    if (!FS.path_exist("$app_data_root$"))
        return;

    string_path cmd_path;
    if (!FS.update_path(cmd_path, "$app_data_root$", "coop_cmd.txt"))
        return;

    if (GetFileAttributesA(cmd_path) == INVALID_FILE_ATTRIBUTES)
        return;

    FILE* f = fopen(cmd_path, "rt");
    if (!f)
        return;

    char command[512] = "";
    if (fgets(command, sizeof(command), f))
    {
        size_t len = xr_strlen(command);
        while (len > 0 && (command[len - 1] == '\r' || command[len - 1] == '\n'))
        {
            command[--len] = 0;
        }
    }
    fclose(f);
    DeleteFileA(cmd_path);

    if (command[0] && Console)
    {
        Msg("* coop: exec from coop_cmd.txt: '%s'", command);
        Console->Execute(command);
    }
}
} // namespace coop