# Two-copy co-op smoke test at level scope: both engines start the same
# single-player game straight from the command line (-exec runs coop_* and the
# start command, no GUI clicks), then the script looks for the handshake and for
# the remote doll each side spawns for the other player.
#
# Usage:
#   ./scripts/test_coop_level.ps1
#   ./scripts/test_coop_level.ps1 -TimeoutSeconds 400 -Keep -ShowLogs
#
# Expects prepared copies (prepare_testcopy.ps1) and a deployed build
# (deploy.ps1). -force_flushlog is required: the engine log is otherwise
# buffered and invisible until the process exits.
#
# Exits 0 only if both sides report the peer and both spawned a remote doll.

param(
    [string]$HostDir = 'local\game-host',
    [string]$ClientDir = 'local\game-client',
    [int]$Port = 27016,
    [string]$Server = 'all/single/alife/new',
    [int]$ClientDelaySeconds = 15,
    [int]$TimeoutSeconds = 300,
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

$hostCopy = Resolve-Copy $HostDir
$clientCopy = Resolve-Copy $ClientDir
$started = Get-Date

# -nogameintro skips the opening cutscene that a new game starts right after the
# level loads. The sequencer freezes actor updates, so nothing coop-related (doll
# spawn, movement) happens until it is skipped by hand; it is also the only thing
# that keeps the run waiting, the level itself loads in ~25 s.
#
# The final argument must be -exec: the engine takes everything after it up to
# the end of the command line, and splits it on ';'.
$hostArgs = @('-cs', '-nointro', '-nogameintro', '-force_flushlog', '-exec',
    "coop_listen $Port; start server($Server)")
$clientArgs = @('-cs', '-nointro', '-nogameintro', '-force_flushlog', '-exec',
    "coop_connect 127.0.0.1:$Port $ClientName; start server($Server)")

Write-Host "==> host:   $($hostCopy.Exe) (listen $Port, start $Server)"
$hostProcess = Start-Process -FilePath $hostCopy.Exe -WorkingDirectory $hostCopy.Root `
    -ArgumentList $hostArgs -PassThru
$clientProcess = $null

try {
    Start-Sleep -Seconds $ClientDelaySeconds
    Write-Host "==> client: $($clientCopy.Exe) (connect 127.0.0.1:$Port, start $Server)"
    $clientProcess = Start-Process -FilePath $clientCopy.Exe -WorkingDirectory $clientCopy.Root `
        -ArgumentList $clientArgs -PassThru

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    $hostLog = $null
    $clientLog = $null
    $checks = @{}
    while ((Get-Date) -lt $deadline) {
        $hostLog = Newest-Log $hostCopy.Logs $started
        $clientLog = Newest-Log $clientCopy.Logs $started
        if ($hostLog) {
            $checks['host listening'] = Test-Pattern $hostLog.FullName 'coop: hosting on port'
            $checks['host sees peer'] = Test-Pattern $hostLog.FullName "coop: peer \d+ \('$ClientName'\) joined"
            $checks['host doll'] = Test-Pattern $hostLog.FullName 'coop: remote player \d+ spawned as game id \d+'
        }
        if ($clientLog) {
            $checks['client joined'] = Test-Pattern $clientLog.FullName 'coop: joined '
            $checks['client doll'] = Test-Pattern $clientLog.FullName 'coop: remote player \d+ spawned as game id \d+'
        }
        if ($checks.Count -gt 0 -and -not ($checks.Values -contains $false)) { break }
        Start-Sleep -Milliseconds 500
    }
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

$order = 'host listening', 'host sees peer', 'client joined', 'host doll', 'client doll'
foreach ($key in $order) {
    $ok = $checks.ContainsKey($key) -and $checks[$key]
    Write-Host ("{0,-16} {1}" -f $key, $(if ($ok) { 'ok' } else { 'FAILED' }))
}

if ($ShowLogs) {
    foreach ($log in @($hostLog, $clientLog)) {
        if (-not $log) { continue }
        Write-Host ''
        Write-Host "--- $($log.FullName)"
        Select-String -Path $log.FullName -Pattern 'coop:|exec:|! coop' |
            Select-Object -Last 30 | ForEach-Object { $_.Line }
    }
}

$ok = -not ($order | Where-Object { -not ($checks.ContainsKey($_) -and $checks[$_]) })
if ($ok) { Write-Host ''; Write-Host 'ALL OK' } else { Write-Host ''; Write-Host 'FAILED' }
exit $(if ($ok) { 0 } else { 1 })