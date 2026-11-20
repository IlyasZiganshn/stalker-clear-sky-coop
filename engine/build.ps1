# Builds the pinned OpenXRay sources fetched by fetch.ps1.
# Usage:  ./engine/build.ps1 [-Configuration Release|Mixed|Debug|"Release Master Gold"] [-Platform x64|x86]
# The game itself is not needed: the engine compiles from source alone.

param(
    [string]$Configuration = 'Release',
    [string]$Platform = 'x64'
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$engineSrc = Join-Path $PSScriptRoot 'src'
$solution = Join-Path $engineSrc 'src\engine.sln'

if (!(Test-Path $solution)) {
    & (Join-Path $PSScriptRoot 'fetch.ps1')
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (!(Test-Path $vswhere)) {
    throw "vswhere not found at $vswhere. Install Visual Studio Build Tools 2022 with the C++ workload."
}

$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vsPath) {
    throw 'No Visual Studio installation with C++ tools found.'
}

$msbuild = Join-Path $vsPath 'MSBuild\Current\Bin\MSBuild.exe'
if (!(Test-Path $msbuild)) {
    throw "MSBuild not found at $msbuild"
}

$nuget = Join-Path $root 'tools\_local\nuget.exe'
if (!(Test-Path $nuget)) {
    New-Item -ItemType Directory -Force -Path (Split-Path $nuget) | Out-Null
    Write-Host '==> download nuget'
    Invoke-WebRequest -Uri https://dist.nuget.org/win-x86-commandline/latest/nuget.exe -OutFile $nuget
}

Write-Host '==> nuget restore'
& $nuget restore $solution -Verbosity quiet
if ($LASTEXITCODE -ne 0) {
    throw "nuget restore failed ($LASTEXITCODE)"
}

Write-Host "==> msbuild ($Configuration $Platform)"
& $msbuild $solution /m "/p:Configuration=$Configuration" "/p:Platform=$Platform" /v:minimal /nologo
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host "engine: build finished ($Configuration $Platform)"