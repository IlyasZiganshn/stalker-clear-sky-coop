# Collects diagnostic pack for Clear Sky Co-op bug reports and sessions:
# - Git commit hash, branch, status, uncommitted diffs
# - Host & client engine logs (_appdata_\logs\*.log)
# - Crash dumps (_appdata_\dumps\*.mdmp)
# - Recent savegames (_appdata_\savedgames\*.sav)
# - User config (user.ltx, fsgame.ltx)
# - System info (OS, CPU, GPU, RAM, network interfaces)
# - Reproduction steps and session notes
#
# Usage:
#   ./scripts/collect_diagnostics.ps1 -Notes "Desync on level change from marsh to escape"
#   ./scripts/collect_diagnostics.ps1 -OutDir diagnostics -Zip

param(
    [string]$OutDir = 'diagnostics',
    [string]$HostDir = 'local\game-host',
    [string]$ClientDir = 'local\game-client',
    [string]$Notes = '',
    [switch]$Zip
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

$timestamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$packName = "coop_diag_$timestamp"
$packDir = Join-Path (Join-Path $root $OutDir) $packName
New-Item -ItemType Directory -Path $packDir -Force | Out-Null

Write-Host "==> Collecting diagnostics into $packDir..."

# 1. Git and build metadata
$meta = [ordered]@{}
try {
    $meta['git_commit'] = (git rev-parse HEAD 2>$null).Trim()
    $meta['git_branch'] = (git rev-parse --abbrev-ref HEAD 2>$null).Trim()
    $meta['git_status'] = (git status -s 2>$null)
    $gitDiff = git diff HEAD 2>$null
    if ($gitDiff) {
        Set-Content -Path (Join-Path $packDir 'git_diff.patch') -Value $gitDiff -Encoding UTF8
        $meta['has_uncommitted_changes'] = $true
    } else {
        $meta['has_uncommitted_changes'] = $false
    }
} catch {
    $meta['git_error'] = $_.Exception.Message
}

# 2. System and environment info
$meta['timestamp_utc'] = (Get-Date).ToUniversalTime().ToString('o')
$meta['os_version'] = [System.Environment]::OSVersion.VersionString
$meta['machine_name'] = $env:COMPUTERNAME
$meta['user_name'] = $env:USERNAME
$meta['cpu'] = (Get-CimInstance Win32_Processor | Select-Object -First 1 -ExpandProperty Name)
$meta['total_ram_gb'] = [math]::Round((Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory / 1GB, 2)
$meta['gpus'] = @(Get-CimInstance Win32_VideoController | Select-Object -ExpandProperty Name)

# Network interfaces
$netInfo = Get-NetIPAddress -AddressFamily IPv4 -ErrorAction SilentlyContinue |
    Where-Object { $_.InterfaceAlias -notlike '*Loopback*' } |
    Select-Object InterfaceAlias, IPAddress, PrefixLength
$meta['network_interfaces'] = $netInfo

# 3. Reproduction steps and user notes
$meta['repro_notes'] = $Notes
$metaJson = $meta | ConvertTo-Json -Depth 5
Set-Content -Path (Join-Path $packDir 'session_info.json') -Value $metaJson -Encoding UTF8

if ($Notes) {
    Set-Content -Path (Join-Path $packDir 'repro_steps.txt') -Value $Notes -Encoding UTF8
}

# 4. Copy game logs, dumps, configs, saves from host and client
function Collect-InstanceData([string]$dir, [string]$role) {
    $resolved = if ([System.IO.Path]::IsPathRooted($dir)) { $dir } else { Join-Path $root $dir }
    $roleDir = Join-Path $packDir $role
    New-Item -ItemType Directory -Path $roleDir -Force | Out-Null

    if (-not (Test-Path $resolved)) {
        Write-Warning "Directory not found: $resolved"
        return
    }

    # Binaries metadata
    $engineExe = Join-Path $resolved 'bin\x64\xrEngine.exe'
    if (Test-Path $engineExe) {
        $exeItem = Get-Item $engineExe
        $exeInfo = [ordered]@{
            Path = $engineExe
            Size = $exeItem.Length
            LastWriteTimeUtc = $exeItem.LastWriteTimeUtc.ToString('o')
            FileVersion = $exeItem.VersionInfo.FileVersion
        }
        $exeInfo | ConvertTo-Json | Set-Content -Path (Join-Path $roleDir 'binary_info.json') -Encoding UTF8
    }

    $appData = Join-Path $resolved '_appdata_'
    if (Test-Path $appData) {
        # Logs
        $logsDir = Join-Path $appData 'logs'
        if (Test-Path $logsDir) {
            $destLogs = Join-Path $roleDir 'logs'
            New-Item -ItemType Directory -Path $destLogs -Force | Out-Null
            Get-ChildItem $logsDir -Filter '*.log' |
                Sort-Object LastWriteTime -Descending |
                Select-Object -First 5 |
                ForEach-Object { Copy-Item $_.FullName -Destination $destLogs -Force }
        }

        # Crash dumps
        $dumpsDir = Join-Path $appData 'dumps'
        if (Test-Path $dumpsDir) {
            $destDumps = Join-Path $roleDir 'dumps'
            New-Item -ItemType Directory -Path $destDumps -Force | Out-Null
            Get-ChildItem $dumpsDir -Filter '*.mdmp' |
                Sort-Object LastWriteTime -Descending |
                Select-Object -First 3 |
                ForEach-Object { Copy-Item $_.FullName -Destination $destDumps -Force }
        }

        # Configs
        $userLtx = Join-Path $appData 'user.ltx'
        if (Test-Path $userLtx) {
            Copy-Item $userLtx -Destination (Join-Path $roleDir 'user.ltx') -Force
        }

        # Latest savegame
        $savesDir = Join-Path $appData 'savedgames'
        if (Test-Path $savesDir) {
            $destSaves = Join-Path $roleDir 'savedgames'
            New-Item -ItemType Directory -Path $destSaves -Force | Out-Null
            Get-ChildItem $savesDir -Filter '*.sav' |
                Sort-Object LastWriteTime -Descending |
                Select-Object -First 2 |
                ForEach-Object { Copy-Item $_.FullName -Destination $destSaves -Force }
        }
    }

    # fsgame.ltx
    $fsgame = Join-Path $resolved 'fsgame.ltx'
    if (Test-Path $fsgame) {
        Copy-Item $fsgame -Destination (Join-Path $roleDir 'fsgame.ltx') -Force
    }
}

Collect-InstanceData $HostDir 'host'
Collect-InstanceData $ClientDir 'client'

# 5. Pack into ZIP if requested
$finalPath = $packDir
if ($Zip) {
    $zipPath = "$packDir.zip"
    Write-Host "==> Compressing into $zipPath..."
    Compress-Archive -Path "$packDir\*" -DestinationPath $zipPath -Force
    Remove-Item -Recurse -Force $packDir
    $finalPath = $zipPath
}

Write-Host "==> Diagnostics collected successfully: $finalPath"
return $finalPath
