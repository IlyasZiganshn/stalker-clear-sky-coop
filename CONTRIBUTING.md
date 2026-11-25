# Contributing to Stalker Clear Sky: Coop

Thank you for your interest in contributing! Please review this guide and our [Code of Conduct](CODE_OF_CONDUCT.md) before opening an issue or pull request.

## Ground Rules

1. **No proprietary game files or binaries**: Never commit files extracted from original S.T.A.L.K.E.R.: Clear Sky (`.db*`, `.xdb`, meshes, textures, sounds, levels), compiled binaries (`bin/`, `build/`, `ClearSkyCoopLauncher.exe`), or local configs/logs (`launcher_config.ini`, `*.log`, `*.sav`).
2. **Preserve the repository layout**:
   - `coopnet/` — standalone C++20 networking library and unit tests.
   - `engine/` — OpenXRay commit pin (`engine/upstream.txt`), patch queue (`engine/patches/`), and helper scripts (`fetch.ps1`, `apply.ps1`, `build.ps1`). Never commit `engine/src/`.
   - `gamedata/scripts/` — custom co-op Lua scripts only.
   - `launcher/` — C# WinForms launcher (`ClearSkyCoopLauncher`).
   - `scripts/` — build, deployment, packaging, and test automation scripts.
   - `tools/` — standalone CLI network test and simulation utilities.
   - `docs/` — architecture, protocol, build, roadmap, and testing documentation.
3. **Prefer `coopnet/` and Lua before engine patches**: Only touch `engine/patches/` when engine internals genuinely cannot be hooked from `coopnet` or `gamedata/scripts/`. Keep patches minimal (`NNNN-topic.patch`) with no unrelated formatting changes.

## Local Development & Verification

### 1. Networking Layer (`coopnet`)
Requires Visual Studio 2022 / Build Tools with C++20, CMake >= 3.25, and Ninja:

```powershell
./scripts/build.ps1 -Test
```

### 2. Launcher (`ClearSkyCoopLauncher`)
Compiles `launcher/src/*.cs` via Roslyn or .NET 4.8 `csc.exe` and runs the built-in UDP lobby discovery self-test:

```powershell
./launcher/build.ps1
./ClearSkyCoopLauncher.exe --test-lobby
```

### 3. Patched OpenXRay Engine (Optional, requires MSVC + Windows SDK)
```powershell
./engine/fetch.ps1
./engine/apply.ps1
./engine/build.ps1
```

## Pull Request Checklist

- Keep changes focused on a single feature or bug fix.
- Verify `./scripts/build.ps1 -Test` and `./launcher/build.ps1` + `--test-lobby` pass locally.
- Do not commit personal paths, machine logs, save files, or secrets.
- Update relevant documentation (`README.md`, `README.ru.md`, or `docs/`) when behavior or setup steps change.
