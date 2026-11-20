# Builds coopnet with the MSVC toolchain bundled in Visual Studio Build Tools.
# Usage:  ./scripts/build.ps1 [-Release] [-Test]
param(
    [switch]$Release,
    [switch]$Test
)

$ErrorActionPreference = 'Stop'

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) {
    throw "vswhere not found at $vswhere. Install Visual Studio Build Tools 2022 with the C++ workload."
}

$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) {
    throw 'No Visual Studio installation with C++ tools found.'
}

Import-Module (Join-Path $vsPath 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $vsPath -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null

$cmakeBin = Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'
$cmake = Join-Path $cmakeBin 'cmake.exe'
$ctest = Join-Path $cmakeBin 'ctest.exe'
if (-not (Test-Path $cmake)) { $cmake = 'cmake' }
if (-not (Test-Path $ctest)) { $ctest = 'ctest' }

$preset = if ($Release) { 'release' } else { 'default' }

Write-Host "==> configure ($preset)"
& $cmake --preset $preset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host '==> build'
& $cmake --build --preset $preset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if ($Test -or -not $Release) {
    Write-Host '==> test'
    & $ctest --preset $preset
    exit $LASTEXITCODE
}