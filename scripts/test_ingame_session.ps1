# In-game smoke test: boots two Clear Sky copies with the OpenXRay build, asks one
# to listen and the other to connect, then looks for the session in the engine
# log of each side. No level is loaded, so this proves the engine runs, the coop
# commands are registered and UDP handshake/keepalive work inside the game.
#
# Usage:
#   ./scripts/test_ingame_session.ps1
#   ./scripts/test_ingame_session.ps1 -Seconds 40 -Keep
#
# Expects the copies to be prepared (prepare_testcopy.ps1) and deployed
# (deploy.ps1). Exits 0 only if both sides report the peer.

param(
    [string]$HostDir = 'local\game-host',
    [string]$ClientDir = 'local\game-client',
    [int]$Port = 27016,
    [int]$BootSeconds = 25,
    [int]$RunSeconds = 25,
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

function Wait-ForPattern([string]$path, [string]$pattern, [int]$timeoutSec) {
    $deadline = (Get-Date).AddSeconds($timeoutSec)
    while ((Get-Date) -lt $deadline) {
        if ((Test-Path $path) -and (Select-String -Path $path -Pattern $pattern -Quiet)) { return $true }
        Start-Sleep -Milliseconds 500
    }
    return $false
}

$host1 = Resolve-Copy $HostDir
$client = Resolve-Copy $ClientDir
$started = Get-Date

Write-Host "==> host: $($host1.Exe) (listen $Port)"
$hostProcess = Start-Process -FilePath $host1.Exe -WorkingDirectory $host1.Root `
    -ArgumentList '-cs', '-nointro', '-force_flushlog', '-exec', "coop_listen $Port" -PassThru

try {
    Start-Sleep -Seconds $BootSeconds
    Write-Host "==> client: $($client.Exe) (connect 127.0.0.1:$Port)"
    $clientProcess = Start-Process -FilePath $client.Exe -WorkingDirectory $client.Root `
        -ArgumentList '-cs', '-nointro', '-force_flushlog', '-exec', "coop_connect 127.0.0.1:$Port Tester" -PassThru

    $runDeadline = (Get-Date).AddSeconds($RunSeconds)
    $hostLog = $null
    $clientLog = $null
    $joined = $false
    $connected = $false
    while ((Get-Date) -lt $runDeadline -and -not ($joined -and $connected)) {
        $hostLog = Newest-Log $host1.Logs $started
        $clientLog = Newest-Log $client.Logs $started
        if (-not $joined -and $hostLog) {
            $joined = Select-String -Path $hostLog.FullName -Pattern "coop: peer \d+ \('Tester'\) joined" -Quiet
        }
        if (-not $connected -and $clientLog) {
            $connected = Select-String -Path $clientLog.FullName -Pattern 'coop: joined ' -Quiet
        }
        if (-not ($joined -and $connected)) { Start-Sleep -Milliseconds 500 }
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

$listening = $hostLog -and (Select-String -Path $hostLog.FullName -Pattern 'coop: hosting on port' -Quiet)
$results = [ordered]@{
    'host listening' = [bool]$listening
    'client joined' = [bool]$connected
    'host sees peer' = [bool]$joined
}

foreach ($key in $results.Keys) {
    Write-Host ("{0,-16} {1}" -f $key, $(if ($results[$key]) { 'ok' } else { 'FAILED' }))
}

if ($ShowLogs) {
    foreach ($log in @($hostLog, $clientLog)) {
        if ($log) {
            Write-Host ''
            Write-Host "--- $($log.FullName)"
            Select-String -Path $log.FullName -Pattern 'coop:|OpenXRay|Clear Sky' | Select-Object -Last 40 | ForEach-Object { $_.Line }
        }
    }
}

$ok = -not ($results.Values -contains $false)
if ($ok) { Write-Host ''; Write-Host 'ALL OK' } else { Write-Host ''; Write-Host 'FAILED' }
exit $(if ($ok) { 0 } else { 1 })