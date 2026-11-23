# Save and load session resilience test:
# 1. Boots host and client with OpenXRay into a live level.
# 2. Waits for handshake and doll spawn on both sides.
# 3. Host executes `save coop_saveload_test` via coop_cmd.txt.
# 4. Verifies save file was created on disk and save completed cleanly.
# 5. Host executes `load coop_saveload_test` via coop_cmd.txt to reload the saved game.
# 6. Verifies host reloads cleanly, doll is respawned, and peer connection survives.
#
# Usage:
#   ./scripts/test_coop_saveload.ps1
#   ./scripts/test_coop_saveload.ps1 -ShowLogs

param(
    [string]$HostDir = 'local\game-host',
    [string]$ClientDir = 'local\game-client',
    [int]$Port = 27016,
    [string]$Server = 'all/single/alife/new',
    [string]$SaveName = 'coop_saveload_test',
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
    return [pscustomobject]@{
        Root = $full;
        Exe = $exe;
        AppData = (Join-Path $full '_appdata_');
        Logs = (Join-Path $full '_appdata_\logs');
        Saves = (Join-Path $full '_appdata_\savedgames')
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

# Clean old save file and cmd file if present
$saveFile = Join-Path $hostCopy.Saves "$SaveName.sav"
if (Test-Path $saveFile) { Remove-Item $saveFile -Force -ErrorAction SilentlyContinue }
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
    # Phase 1: Initial connection
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
            $checks['initial doll spawn'] = Test-Pattern $hostLog.FullName 'coop: remote player \d+ spawned as game id \d+'
        }
        if ($clientLog) {
            $checks['initial client join'] = Test-Pattern $clientLog.FullName 'coop: joined '
        }

        if ($checks.Count -ge 4 -and -not ($checks.Values -contains $false)) { break }
        Start-Sleep -Milliseconds 500
    }

    Write-Host "==> Phase 1 complete: peer connected and doll spawned."

    # Give world 3 seconds to settle
    Start-Sleep -Seconds 3

    # Phase 2: Host saves the game
    Write-Host "==> Phase 2: Host saving game as '$SaveName'..."
    Send-CoopCmd $hostCopy "save $SaveName"

    $deadlineSave = (Get-Date).AddSeconds(15)
    $saveCompleted = $false
    while ((Get-Date) -lt $deadlineSave) {
        if (Test-Path $saveFile) {
            $saveCompleted = $true
            break
        }
        Start-Sleep -Milliseconds 500
    }

    $checks['host save executed'] = if ($hostLog) { Test-Pattern $hostLog.FullName "exec from coop_cmd.txt: 'save $SaveName'" } else { $false }
    $checks['save file created'] = $saveCompleted

    if (-not $saveCompleted) {
        throw "save file was not created within deadline ($saveFile)"
    }

    Write-Host "==> Phase 2 complete: save file created ($((Get-Item $saveFile).Length) bytes)."

    # Give 2 seconds before reloading
    Start-Sleep -Seconds 2

    # Phase 3: Host reloads the saved game
    Write-Host "==> Phase 3: Host loading game '$SaveName'..."
    Send-CoopCmd $hostCopy "load $SaveName"

    $deadlineLoad = (Get-Date).AddSeconds($TimeoutSeconds / 2)
    $reloadCompleted = $false
    $dollRespawned = $false

    while ((Get-Date) -lt $deadlineLoad) {
        if ($hostLog) {
            # Check for second local spawn publish or reload message
            $spawnPublishCount = (Select-String -Path $hostLog.FullName -Pattern 'coop: local spawn published').Count
            $remoteSpawnCount = (Select-String -Path $hostLog.FullName -Pattern 'coop: remote player \d+ spawned as game id \d+').Count
            if ($spawnPublishCount -ge 2) { $reloadCompleted = $true }
            if ($remoteSpawnCount -ge 2) { $dollRespawned = $true }
        }

        $checks['host load executed'] = if ($hostLog) { Test-Pattern $hostLog.FullName "exec from coop_cmd.txt: 'load $SaveName'" } else { $false }
        $checks['host reload completed'] = $reloadCompleted
        $checks['doll respawned after reload'] = $dollRespawned
        $checks['client survived reload'] = if ($clientProcess) { -not $clientProcess.HasExited } else { $false }

        if ($reloadCompleted -and $dollRespawned) { break }
        Start-Sleep -Milliseconds 500
    }

    Write-Host "==> Phase 3 complete: game reloaded and remote doll respawned."
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
Write-Host "==> Save/Load verification checks:"
foreach ($key in $checks.Keys) {
    $ok = [bool]$checks[$key]
    Write-Host ("  {0,-28} {1}" -f $key, $(if ($ok) { 'ok' } else { 'FAILED' }))
}

if ($ShowLogs) {
    if ($hostLog) {
        Write-Host ''
        Write-Host "--- $($hostLog.FullName)"
        Select-String -Path $hostLog.FullName -Pattern 'coop:|exec:|! coop|\.sav' |
            Select-Object -Last 35 | ForEach-Object { $_.Line }
    }
}

$ok = -not ($checks.Values -contains $false) -and ($checks.Count -ge 9)
if ($ok) {
    Write-Host ''
    Write-Host 'ALL SAVE/LOAD CHECKS PASSED (save/load session resilience verified)'
    exit 0
} else {
    Write-Host ''
    Write-Host 'FAILED'
    exit 1
}
