# Reconnect and session resilience test:
# 1. Boots host and client with OpenXRay into a live level.
# 2. Waits for handshake and doll spawn on both sides.
# 3. Kills client process (unexpected disconnect / network drop).
# 4. Host detects peer disappearance, despawns remote doll cleanly (no leaks/crashes).
# 5. Boots a new client that reconnects to the still-running host.
# 6. Verifies that the host accepts reconnect and spawns the doll again.
#
# Usage:
#   ./scripts/test_coop_reconnect.ps1
#   ./scripts/test_coop_reconnect.ps1 -ShowLogs

param(
    [string]$HostDir = 'local\game-host',
    [string]$ClientDir = 'local\game-client',
    [int]$Port = 27016,
    [string]$Server = 'all/single/alife/new',
    [int]$ClientDelaySeconds = 15,
    [int]$TimeoutSeconds = 180,
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
    return [pscustomobject]@{ Root = $full; Exe = $exe; Logs = (Join-Path $full '_appdata_\logs') }
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

# Clean up any lingering processes
Get-Process xrEngine -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 500

$hostCopy = Resolve-Copy $HostDir
$clientCopy = Resolve-Copy $ClientDir

# Clean previous logs to avoid stale matches
if (Test-Path $hostCopy.Logs) { Get-ChildItem $hostCopy.Logs -Filter '*.log' | Remove-Item -Force -ErrorAction SilentlyContinue }
if (Test-Path $clientCopy.Logs) { Get-ChildItem $clientCopy.Logs -Filter '*.log' | Remove-Item -Force -ErrorAction SilentlyContinue }

$started = Get-Date

$hostArgs = @('-cs', '-nointro', '-nogameintro', '-force_flushlog', '-exec',
    "coop_listen $Port; start server($Server)")
$clientArgs = @('-cs', '-nointro', '-nogameintro', '-force_flushlog', '-exec',
    "coop_connect 127.0.0.1:$Port $ClientName; start server($Server)")

Write-Host "==> host:   $($hostCopy.Exe) (listen $Port)"
$hostProcess = Start-Process -FilePath $hostCopy.Exe -WorkingDirectory $hostCopy.Root `
    -ArgumentList $hostArgs -PassThru
$clientProcess1 = $null
$clientProcess2 = $null

$checks = [ordered]@{}

try {
    # Phase 1: Initial connection
    Start-Sleep -Seconds $ClientDelaySeconds
    Write-Host "==> client 1: $($clientCopy.Exe) (connect 127.0.0.1:$Port)"
    $clientProcess1 = Start-Process -FilePath $clientCopy.Exe -WorkingDirectory $clientCopy.Root `
        -ArgumentList $clientArgs -PassThru

    $deadline1 = (Get-Date).AddSeconds($TimeoutSeconds / 2)
    $hostLog = $null
    $clientLog1 = $null

    while ((Get-Date) -lt $deadline1) {
        $hostLog = Newest-Log $hostCopy.Logs $started
        $clientLog1 = Newest-Log $clientCopy.Logs $started

        if ($hostLog) {
            $checks['initial host listen'] = Test-Pattern $hostLog.FullName 'coop: hosting on port'
            $checks['initial peer join'] = Test-Pattern $hostLog.FullName "coop: peer \d+ \('$ClientName'\) joined"
            $checks['initial doll spawn'] = Test-Pattern $hostLog.FullName 'coop: remote player \d+ spawned as game id \d+'
        }
        if ($clientLog1) {
            $checks['initial client join'] = Test-Pattern $clientLog1.FullName 'coop: joined '
        }

        if ($checks.Count -ge 4 -and -not ($checks.Values -contains $false)) { break }
        Start-Sleep -Milliseconds 500
    }

    Write-Host "==> Phase 1 complete: peer connected and doll spawned."

    # Phase 2: Kill client and observe timeout / despawn on host
    Write-Host "==> Killing client 1 to simulate disconnect / crash..."
    if ($clientProcess1 -and -not $clientProcess1.HasExited) {
        Stop-Process -Id $clientProcess1.Id -Force
    }

    Write-Host "==> Waiting for host to detect timeout and despawn doll (~5s)..."
    $deadlineDespawn = (Get-Date).AddSeconds(20)
    $despawnObserved = $false
    while ((Get-Date) -lt $deadlineDespawn) {
        if ($hostLog) {
            # Check for despawn or timeout message in host log
            $despawnObserved = Test-Pattern $hostLog.FullName 'coop: remote player \d+ removed'
            if ($despawnObserved) { break }
        }
        Start-Sleep -Milliseconds 500
    }
    $checks['host doll despawned'] = $despawnObserved

    # Phase 3: Reconnect with client 2
    Start-Sleep -Seconds 2
    if (Test-Path $clientCopy.Logs) { Get-ChildItem $clientCopy.Logs -Filter '*.log' | Remove-Item -Force -ErrorAction SilentlyContinue }
    $reconnectStarted = Get-Date
    Write-Host "==> client 2: reconnecting to running host..."
    $clientProcess2 = Start-Process -FilePath $clientCopy.Exe -WorkingDirectory $clientCopy.Root `
        -ArgumentList $clientArgs -PassThru

    $deadline2 = (Get-Date).AddSeconds($TimeoutSeconds / 2)
    $clientLog2 = $null
    $reconnectJoinObserved = $false
    $reconnectSpawnObserved = $false
    $client2Joined = $false

    while ((Get-Date) -lt $deadline2) {
        $clientLog2 = Newest-Log $clientCopy.Logs $reconnectStarted

        if ($hostLog) {
            # Search for second peer join and second remote spawn
            $joinCount = (Select-String -Path $hostLog.FullName -Pattern "coop: peer \d+ \('$ClientName'\) joined").Count
            $spawnCount = (Select-String -Path $hostLog.FullName -Pattern 'coop: remote player \d+ spawned as game id \d+').Count
            if ($joinCount -ge 2) { $reconnectJoinObserved = $true }
            if ($spawnCount -ge 2) { $reconnectSpawnObserved = $true }
        }
        if ($clientLog2) {
            if (Test-Pattern $clientLog2.FullName 'coop: joined ') { $client2Joined = $true }
        }

        $checks['reconnect peer join'] = $reconnectJoinObserved
        $checks['reconnect doll spawn'] = $reconnectSpawnObserved
        $checks['reconnect client join'] = $client2Joined

        if ($reconnectJoinObserved -and $reconnectSpawnObserved -and $client2Joined) { break }
        Start-Sleep -Milliseconds 500
    }
}
finally {
    if (-not $Keep) {
        foreach ($p in @($hostProcess, $clientProcess1, $clientProcess2)) {
            if ($p -and -not $p.HasExited) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }
        }
    }
}

Write-Host ''
Write-Host "host log:   $(if ($hostLog) { $hostLog.FullName } else { 'none' })"
Write-Host ''
Write-Host "==> Reconnect verification checks:"
foreach ($key in $checks.Keys) {
    $ok = [bool]$checks[$key]
    Write-Host ("  {0,-24} {1}" -f $key, $(if ($ok) { 'ok' } else { 'FAILED' }))
}

if ($ShowLogs) {
    if ($hostLog) {
        Write-Host ''
        Write-Host "--- $($hostLog.FullName)"
        Select-String -Path $hostLog.FullName -Pattern 'coop:|exec:|! coop' |
            Select-Object -Last 35 | ForEach-Object { $_.Line }
    }
}

$ok = -not ($checks.Values -contains $false) -and ($checks.Count -ge 8)
if ($ok) {
    Write-Host ''
    Write-Host 'ALL RECONNECT CHECKS PASSED (reconnect resilience verified)'
    exit 0
} else {
    Write-Host ''
    Write-Host 'FAILED'
    exit 1
}
