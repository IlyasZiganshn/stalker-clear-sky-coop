# Prepares a game copy for running the OpenXRay build: replaces the game's
# fsgame.ltx with the engine's own root map so that logs, saves and the shader
# cache land in <copy>\_appdata_ instead of a path baked into the installer.
#
# Two copies with the same app_data_root would fight over logs and saves, which
# is why every test copy gets its own.
#
# Usage:
#   ./scripts/prepare_testcopy.ps1 -GameDir 'local\game-host'
#   ./scripts/prepare_testcopy.ps1 -GameDir 'local\game-client' -DryRun

param(
    [Parameter(Mandatory = $true)][string]$GameDir,
    [switch]$DryRun
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$engineFsgame = Join-Path $root 'engine\src\res\fsgame.ltx'

if (-not (Test-Path $GameDir)) {
    Write-Host "prepare: game directory not found: $GameDir"
    exit 1
}
foreach ($part in 'bin', 'resources', 'levels') {
    if (-not (Test-Path (Join-Path $GameDir $part))) {
        Write-Host "prepare: $GameDir has no $part; is this a Clear Sky install?"
        exit 1
    }
}
if (-not (Test-Path $engineFsgame)) {
    Write-Host "prepare: $engineFsgame is missing, run ./engine/fetch.ps1 first"
    exit 1
}

$target = Join-Path $GameDir 'fsgame.ltx'
$backup = Join-Path $GameDir 'fsgame.ltx.orig'

if (Test-Path $target) {
    $current = Get-Content $target -Raw
    $wanted = Get-Content $engineFsgame -Raw
    if ($current -eq $wanted) {
        Write-Host "prepare: $GameDir already uses the engine root map"
        exit 0
    }
    if (Test-Path $backup) {
        Write-Host 'prepare: previous fsgame.ltx.orig kept, not overwritten'
    }
    elseif ($DryRun) {
        Write-Host "prepare: would back up $target -> $backup"
    }
    else {
        Copy-Item $target $backup
    }
}

if ($DryRun) {
    Write-Host "prepare: would copy $engineFsgame -> $target"
    Write-Host "prepare: dry run, $GameDir unchanged"
    exit 0
}

Copy-Item $engineFsgame $target -Force
Write-Host "prepare: $GameDir uses the engine root map (app data in _appdata_)"