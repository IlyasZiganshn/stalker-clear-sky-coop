# Copies the freshly built engine into a Clear Sky installation so that the
# game can be started from that directory. Only binaries are touched: gamedata,
# saves and user files are left alone.
#
# Usage:
#   ./scripts/deploy.ps1 -GameDir 'D:\games\cs-host'
#   ./scripts/deploy.ps1 -GameDir 'D:\games\cs-client' -DryRun
#   ./scripts/deploy.ps1 -GameDir 'X' -Target 'X\bin\x64'

param(
    [Parameter(Mandatory = $true)][string]$GameDir,
    [string]$Target,
    [switch]$DryRun,
    # Debug symbols are useful when a crash needs a stack, but xrGame.pdb alone
    # is most of a gigabyte per copy: only copy them when asked.
    [switch]$IncludeSymbols
)

$ErrorActionPreference = 'Stop'

$source = Join-Path (Split-Path -Parent $PSScriptRoot) 'engine\src\bin\x64\Release'
if (-not (Test-Path (Join-Path $source 'xrEngine.exe'))) {
    Write-Host "deploy: no built engine in $source, run ./engine/build.ps1 first"
    exit 1
}
if (-not (Test-Path $GameDir)) {
    Write-Host "deploy: game directory not found: $GameDir"
    exit 1
}
if (-not (Test-Path (Join-Path $GameDir 'gamedata'))) {
    Write-Warning "deploy: $GameDir has no gamedata subdirectory"
}

if (-not $Target) {
    $Target = Join-Path $GameDir 'bin\x64'
}

if (-not (Test-Path $Target)) {
    if ($DryRun) {
        Write-Host "deploy: would create $Target"
    } else {
        New-Item -ItemType Directory -Path $Target -Force | Out-Null
    }
}

$copied = 0
foreach ($file in Get-ChildItem $source -File) {
    if (-not $IncludeSymbols -and $file.Extension -eq '.pdb') { continue }
    $destination = Join-Path $Target $file.Name
    if ($DryRun) {
        Write-Host "deploy: $($file.Name) -> $destination"
        $copied++
        continue
    }
    Copy-Item $file.FullName $destination -Force
    $copied++
}

if (-not (Test-Path (Join-Path $GameDir 'fsgame.ltx'))) {
    Write-Host "deploy: no fsgame.ltx in $GameDir; OpenXRay needs its own root map"
}

# Deploy co-op Lua layer if present in repo
$gamedataSource = Join-Path (Split-Path -Parent $PSScriptRoot) 'gamedata'
if (Test-Path $gamedataSource) {
    $targetGamedata = Join-Path $GameDir 'gamedata'
    if ($DryRun) {
        Write-Host "deploy: would copy gamedata to $targetGamedata"
    } else {
        Copy-Item $gamedataSource -Destination $GameDir -Recurse -Force
        Write-Host "deploy: copied co-op gamedata layer to $targetGamedata"
    }
}

if ($DryRun) {
    Write-Host "deploy: dry run, $copied file(s) would be copied to $Target"
} else {
    Write-Host "deploy: copied $copied file(s) to $Target"
}