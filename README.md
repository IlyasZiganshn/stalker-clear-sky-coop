# Stalker Clear Sky: Coop

**[English](README.md) | [Русский](README.ru.md)**

Cooperative multiplayer campaign framework (2–4 players) for **S.T.A.L.K.E.R.: Clear Sky 1.5.10**, built on a custom C++20 UDP transport layer (`coopnet`), targeted patches to **OpenXRay**, synchronized Lua gameplay scripts, and a standalone **Clear Sky Tactical PDA** Windows launcher.

---

## Overview & Capabilities

| Component | Role |
|---|---|
| `coopnet/` | Standalone C++20 reliable/unreliable UDP transport, framing, handshake, delta snapshots, RTT measurement, movement prediction, and actor interpolation. |
| `engine/` | Reproducible patch queue (`engine/patches/*.patch` + `engine/patches/files/`) applied over pinned OpenXRay commit (`engine/upstream.txt`). |
| `gamedata/scripts/` | Lua co-op gameplay layer: story task sync (`coop_tasks.script`), dialog gating (`coop_dialogs.script`), story cutscene sync (`coop_cutscenes.script`), and in-game Ready/Skin lobby (`coop_lobby.script`). |
| `launcher/` | C# WinForms launcher (`ClearSkyCoopLauncher.exe`) with 4 tactical views (`Lobby & Radar`, `Host Campaign`, `Mod & Config`, `Diagnostics`), LAN beacon discovery, mod deployment, and diagnostics bundle export. |
| `tools/` & `scripts/` | Network relay simulator (`net_sim`), standalone echo/snapshot/movement test binaries, release packager, and 2-player live test harnesses. |

### Key Implemented Gameplay Features
- **Host-Authoritative Multiplayer Session**: Direct UDP co-op host/join flow with automatic player spawn sync, remote actor animation/pose interpolation, and level-matching verification.
- **Cooperative Interaction & Survival**: Shared squad HUD/minimap marker & overhead health/nameplate, reduced friendly fire, downed-state ally revival/exchange, and story progression sync (tasks, dialogs, infoportions, cutscenes).
- **In-Game Pre-Match Lobby (`coop_lobby.script`)**: Players select character skins inside the game (not in the launcher) and click **`Ready`**. Changing skins resets **`Ready`** state. When a saved game exists on the Host, only **`Continue`** is shown; when no save exists, both **`Continue`** and **`New Game`** are shown. The Host can start gameplay only after all players are marked **`Ready`**.

---

## Legal Notice & Prerequisites

- **No copyrighted GSC Game World assets are distributed in this repository.** Original game archives (`.db*`/`.xdb`), textures, meshes, levels, and sounds are intentionally excluded.
- Each player must own a **legal installation of S.T.A.L.K.E.R.: Clear Sky (version 1.5.10, Steam / GOG / Retail)**.
- This mod is non-commercial fan software following [GSC Game World's Fan Content Creation Guidelines](https://www.gsc-game.com/guidelines/).

### System Requirements
- **OS**: Windows 10 / 11 (64-bit)
- **Game**: S.T.A.L.K.E.R.: Clear Sky 1.5.10
- **Network**: LAN or virtual LAN (Radmin VPN, ZeroTier, Hamachi, WireGuard) or port forwarding (`27016` UDP / `27015` UDP broadcast)

---

## Installation & Playing (Players)

1. Download or extract the packaged co-op release (or build from source using the steps below).
2. Run **`ClearSkyCoopLauncher.exe`**.
3. Open **Mod & Config**: confirm the path to your S.T.A.L.K.E.R.: Clear Sky installation and deploy the co-op binaries & `gamedata/scripts/`.
4. **Host**: Go to **Host Campaign**, set player name, server name, and UDP port (`27016`), and click **Start Host**.
5. **Client**: Go to **Lobby & Radar**, select a discovered LAN host (or enter Host IP/port directly), and click **Join**.
6. Inside the **In-Game Lobby**, each player chooses a skin and clicks **`Ready`**. Once everyone is ready, the Host clicks **`Continue`** or **`New Game`**.

See [docs/INSTALL.md](docs/INSTALL.md) for detailed troubleshooting and player instructions.

---

## Building & Testing from Source

### 1. Build & Test `coopnet` (No game install required)
Requires **Visual Studio 2022 / Build Tools** (MSVC C++20), **CMake >= 3.25**, and **Ninja**:

```powershell
./scripts/build.ps1 -Test
```

This compiles `coopnet` plus standalone verification tools (`coopnet_tests`, `coopnet_echo`, `coopnet_snap`, `coopnet_move`, `coopnet_chat`, `coopnet_world`, `coopnet_spawn`, `net_sim`) and executes the `ctest` suite.

### 2. Build & Test the Launcher
Requires .NET Framework 4.8 SDK / Visual Studio Roslyn `csc.exe`:

```powershell
./launcher/build.ps1
./ClearSkyCoopLauncher.exe --test-lobby
```

### 3. Build the Patched OpenXRay Engine
```powershell
./engine/fetch.ps1   # clones pinned OpenXRay upstream commit into engine/src/
./engine/apply.ps1   # copies coopnet/ + engine/patches/files/ and applies patches
./engine/build.ps1   # restores packages and compiles Release x64 binaries
```

More build details: [docs/BUILD.md](docs/BUILD.md) and [engine/README.md](engine/README.md).

---

## Repository Structure

```text
.
├── .github/workflows/   CI pipeline (coopnet CMake/ctest + launcher build & lobby test)
├── coopnet/             Standalone C++20 multiplayer transport, state sync & unit tests
├── docs/                Architecture, wire protocol, roadmap, build, install & test guides
├── engine/              Pinned upstream OpenXRay commit & ordered patches (no engine/src in git)
├── gamedata/            Co-op Lua scripts only (no proprietary GSC assets)
├── launcher/            C# WinForms Clear Sky Tactical PDA launcher
├── scripts/             Build, deployment, packaging & live 2-player verification scripts
└── tools/               UDP relay simulator and coopnet integration test binaries
```

---

## Documentation Index

- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) — module boundaries & engine hook design
- [docs/PROTOCOL.md](docs/PROTOCOL.md) — UDP wire framing, messages & snapshots
- [docs/ROADMAP.md](docs/ROADMAP.md) — development milestones (M0–M6)
- [docs/BUILD.md](docs/BUILD.md) — toolchain setup & compilation guide
- [docs/INSTALL.md](docs/INSTALL.md) — end-user setup & gameplay guide
- [docs/TESTING.md](docs/TESTING.md) — automated & manual multiplayer test checklist
- [docs/MEASUREMENTS.md](docs/MEASUREMENTS.md) — performance and latency measurements

---

## Contributing & License

Contributions are welcome! Please read [CONTRIBUTING.md](CONTRIBUTING.md) and our [Code of Conduct](CODE_OF_CONDUCT.md) before opening a PR.

Licensed under the [MIT License](LICENSE).
