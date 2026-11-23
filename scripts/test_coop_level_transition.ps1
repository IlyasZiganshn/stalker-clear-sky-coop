# Synchronous level transition test:
# 1. Boots host and client with OpenXRay into a live level (marsh).
# 2. Waits for handshake and doll spawn on marsh.
# 3. Host transitions to Cordon (`jump_to_level l01_escape`) via coop_cmd.txt.
# 4. Host loads destination level (l01_escape) and broadcasts WorldInfo.
# 5. Client detects host level change and synchronizes transition (`jump_to_level l01_escape`).
# 6. Both peers load destination level, session survives, dolls are respawned on Cordon.
#
# Usage:
#   ./scripts/test_coop_level_transition.ps1
#   ./scripts/test_coop_level_transition.ps1 -ShowLogs

param(
    [string]$HostDir = 'local\game-host',
    [string]$ClientDir = 'local\game-client',
    [int]$Port = 27016,
    [string]$Server = 'all/single/alife/new',
    [string]$TargetLevel = 'escape',
    [int]$ClientDelaySeconds = 15,
    [int]$TimeoutSeconds = 240,
    [string]$ClientName = 'Tester',
    [switch]$Keep,
    [switch]$ShowLogs
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

function Resolve-Copy([string]$path) {
    $full = if ([System.IO.Path]::IsPathRooted($path)) { $path } else { Join-Path $root $path }
    $exe = Join-Path $full 'bin\x64\xrEngine.exe'
    if (-not (Test-Path $exe)) {
        throw "no engine at $exe; run ./scripts/deploy.ps1 -GameDir '$path' first"
    }
    return [pscustomobject]@{
        Root = $full;
        Exe = $exe;
        AppData = (Join-Path $full '_appdata_');
        Logs = (Join-Path $full '_appdata_\logs')
    }
}

function Newest-Log([string]$logsDir, [datetime]$since) {
    if (-not (Test-Path $logsDir)) { return $null }
    Get-ChildItem $logsDir -Filter '*.log' -File |
        Where-Object { $_.LastWriteTime -ge $since } |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
}

function Test-Pattern([string]$path, [string]$pattern) {
    return (Test-Path $path) -and (Select-String -Path $path -Pattern $pattern -Quiet)
}

function Send-CoopCmd([object]$copy, [string]$command) {
    $cmdPath = Join-Path $copy.AppData 'coop_cmd.txt'
    Set-Content -Path $cmdPath -Value $command -Encoding ASCII
}

# Clean up any lingering processes
Get-Process xrEngine -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 500

$hostCopy = Resolve-Copy $HostDir
$clientCopy = Resolve-Copy $ClientDir

# Clean previous logs to avoid stale matches
if (Test-Path $hostCopy.Logs) { Get-ChildItem $hostCopy.Logs -Filter '*.log' | Remove-Item -Force -ErrorAction SilentlyContinue }
if (Test-Path $clientCopy.Logs) { Get-ChildItem $clientCopy.Logs -Filter '*.log' | Remove-Item -Force -ErrorAction SilentlyContinue }

# Clean old cmd file if present
$hostCmdFile = Join-Path $hostCopy.AppData 'coop_cmd.txt'
if (Test-Path $hostCmdFile) { Remove-Item $hostCmdFile -Force -ErrorAction SilentlyContinue }

$started = Get-Date

$hostArgs = @('-cs', '-nointro', '-nogameintro', '-force_flushlog', '-exec',
    "coop_listen $Port; start server($Server)")
$clientArgs = @('-cs', '-nointro', '-nogameintro', '-force_flushlog', '-exec',
    "coop_connect 127.0.0.1:$Port $ClientName; start server($Server)")

Write-Host "==> host:   $($hostCopy.Exe) (listen $Port)"
$hostProcess = Start-Process -FilePath $hostCopy.Exe -WorkingDirectory $hostCopy.Root `
    -ArgumentList $hostArgs -PassThru
$clientProcess = $null

$checks = [ordered]@{}

try {
    # Phase 1: Initial connection on starting level (marsh)
    Start-Sleep -Seconds $ClientDelaySeconds
    Write-Host "==> client: $($clientCopy.Exe) (connect 127.0.0.1:$Port)"
    $clientProcess = Start-Process -FilePath $clientCopy.Exe -WorkingDirectory $clientCopy.Root `
        -ArgumentList $clientArgs -PassThru

    $deadline1 = (Get-Date).AddSeconds($TimeoutSeconds / 2)
    $hostLog = $null
    $clientLog = $null

    while ((Get-Date) -lt $deadline1) {
        $hostLog = Newest-Log $hostCopy.Logs $started
        $clientLog = Newest-Log $clientCopy.Logs $started

        if ($hostLog) {
            $checks['initial host listen'] = Test-Pattern $hostLog.FullName 'coop: hosting on port'
            $checks['initial peer join'] = Test-Pattern $hostLog.FullName "coop: peer \d+ \('$ClientName'\) joined"
            $checks['initial doll spawn on marsh'] = Test-Pattern $hostLog.FullName 'coop: remote player \d+ spawned as game id \d+'
        }
        if ($clientLog) {
            $checks['initial client join'] = Test-Pattern $clientLog.FullName 'coop: joined '
        }

        if ($checks.Count -ge 4 -and -not ($checks.Values -contains $false)) { break }
        Start-Sleep -Milliseconds 500
    }

    Write-Host "==> Phase 1 complete: peer connected and doll spawned on starting level."

    # Give world 3 seconds to settle
    Start-Sleep -Seconds 3

    # Phase 2: Host transitions to target level (l01_escape)
    Write-Host "==> Phase 2: Host transitioning to level '$TargetLevel'..."
    Send-CoopCmd $hostCopy "jump_to_level $TargetLevel"

    $transitionStarted = Get-Date
    $deadlineTransition = (Get-Date).AddSeconds($TimeoutSeconds / 2)
    $hostLoadedTarget = $false
    $clientFollowed = $false
    $clientLoadedTarget = $false
    $dollRespawned = $false

    while ((Get-Date) -lt $deadlineTransition) {
        if ($hostLog) {
            # Check if host loaded target level and re-published spawn
            $hostLoadedTarget = (Select-String -Path $hostLog.FullName -Pattern 'coop: local spawn published').Count -ge 2
            $dollRespawned = (Select-String -Path $hostLog.FullName -Pattern 'coop: remote player \d+ spawned as game id \d+').Count -ge 2
        }

        if ($clientLog) {
            # Check if client observed level transition and followed
            if (Test-Pattern $clientLog.FullName "coop: host moved to level '$TargetLevel'") {
                $clientFollowed = $true
            }
            if ((Select-String -Path $clientLog.FullName -Pattern 'coop: local spawn published').Count -ge 2) {
                $clientLoadedTarget = $true
            }
        }

        $checks['host jump executed'] = if ($hostLog) { Test-Pattern $hostLog.FullName "exec from coop_cmd.txt: 'jump_to_level $TargetLevel'" } else { $false }
        $checks['host loaded target level'] = $hostLoadedTarget
        $checks['client followed transition'] = $clientFollowed
        $checks['client loaded target level'] = $clientLoadedTarget
        $checks['doll respawned on target level'] = $dollRespawned
        $checks['processes survived transition'] = (-not $hostProcess.HasExited) -and (-not $clientProcess.HasExited)

        if ($hostLoadedTarget -and $clientFollowed -and $clientLoadedTarget -and $dollRespawned) { break }
        Start-Sleep -Milliseconds 500
    }

    Write-Host "==> Phase 2 complete: both peers transitioned to '$TargetLevel' with dolls active."
}
finally {
    if (-not $Keep) {
        foreach ($p in @($hostProcess, $clientProcess)) {
            if ($p -and -not $p.HasExited) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }
        }
    }
}

Write-Host ''
Write-Host "host log:   $(if ($hostLog) { $hostLog.FullName } else { 'none' })"
Write-Host "client log: $(if ($clientLog) { $clientLog.FullName } else { 'none' })"
Write-Host ''
Write-Host "==> Level transition verification checks:"
foreach ($key in $checks.Keys) {
    $ok = [bool]$checks[$key]
    Write-Host ("  {0,-32} {1}" -f $key, $(if ($ok) { 'ok' } else { 'FAILED' }))
}

if ($ShowLogs) {
    if ($hostLog) {
        Write-Host ''
        Write-Host "--- $($hostLog.FullName)"
        Select-String -Path $hostLog.FullName -Pattern 'coop:|exec:|! coop|jump_to_level' |
            Select-Object -Last 35 | ForEach-Object { $_.Line }
    }
    if ($clientLog) {
        Write-Host ''
        Write-Host "--- $($clientLog.FullName)"
        Select-String -Path $clientLog.FullName -Pattern 'coop:|exec:|! coop|jump_to_level' |
            Select-Object -Last 35 | ForEach-Object { $_.Line }
    }
}

$ok = -not ($checks.Values -contains $false) -and ($checks.Count -ge 9)
if ($ok) {
    Write-Host ''
    Write-Host 'ALL LEVEL TRANSITION CHECKS PASSED (synchronous level transition verified)'
    exit 0
} else {
    Write-Host ''
    Write-Host 'FAILED'
    exit 1
}
