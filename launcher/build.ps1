# Builds ClearSkyCoopLauncher.exe
# Compiles src/*.cs into standalone Windows executable with Roslyn or .NET 4.8 csc.

param(
    [string]$OutDir = 'bin',
    [switch]$ConsoleMode
)

$ErrorActionPreference = 'Stop'
$launcherDir = $PSScriptRoot
$srcDir = Join-Path $launcherDir 'src'
$targetDir = if ([System.IO.Path]::IsPathRooted($OutDir)) { $OutDir } else { Join-Path (Split-Path -Parent $launcherDir) $OutDir }

if (-not (Test-Path $targetDir)) {
    New-Item -ItemType Directory -Path $targetDir -Force | Out-Null
}

$outExe = Join-Path $targetDir 'ClearSkyCoopLauncher.exe'

# Locate csc.exe
$csc = $null
$candidates = @(
    "D:\PO\VSBuildTools\MSBuild\Current\Bin\Roslyn\csc.exe",
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\Roslyn\csc.exe",
    "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\Roslyn\csc.exe",
    "C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe"
)

foreach ($c in $candidates) {
    if (Test-Path $c) {
        $csc = $c
        break
    }
}

if (-not $csc) {
    throw "csc.exe (C# compiler) not found. Please ensure Build Tools or .NET Framework is installed."
}

Write-Host "==> Compiling ClearSkyCoopLauncher using $csc..."

$sources = Get-ChildItem -Path $srcDir -Filter '*.cs' | ForEach-Object { $_.FullName }
$targetType = if ($ConsoleMode) { 'exe' } else { 'winexe' }

$cscArgs = @(
    "-target:$targetType",
    "-out:$outExe",
    "-platform:x64",
    "-optimize+",
    "-r:System.dll",
    "-r:System.Core.dll",
    "-r:System.Windows.Forms.dll",
    "-r:System.Drawing.dll"
) + $sources

& $csc @cscArgs

if ($LASTEXITCODE -ne 0) {
    throw "Compilation failed with exit code $LASTEXITCODE"
}

Write-Host "==> ClearSkyCoopLauncher built successfully: $outExe"

# Also copy to repo root if built to bin
$rootExe = Join-Path (Split-Path -Parent $launcherDir) 'ClearSkyCoopLauncher.exe'
if ($outExe -ne $rootExe) {
    Copy-Item -Path $outExe -Destination $rootExe -Force
    Write-Host "==> Copied launcher to root: $rootExe"
}

return $outExe
