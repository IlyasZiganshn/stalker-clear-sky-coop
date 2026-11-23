# Motion and input replication acceptance test under network delay and packet loss:
# Boots host and client with OpenXRay, connects client through net_sim relay
# (40 ms delay = 80 ms RTT, 2% loss), runs an automated drive script on client
# (run, crouch, jump, sprint), and asserts that the host observes the remote doll
# moving, crouching, jumping, and sprinting in its authoritative physics world.
#
# Usage:
#   ./scripts/test_coop_motion.ps1
#   ./scripts/test_coop_motion.ps1 -ShowLogs
#
# Exits 0 only if all criteria (session, doll spawn, motion, crouch, jump, sprint) pass.

param(
    [string]$HostDir = 'local\game-host',
    [string]$ClientDir = 'local\game-client',
    [int]$HostPort = 27016,
    [int]$RelayPort = 27017,
    [int]$DelayMs = 40,
    [int]$LossPercent = 2,
    [string]$Server = 'all/single/alife/new',
    [string]$DriveScript = '2000 4000f,2000fc,800j,3000ft',
    [int]$TraceIntervalMs = 200,
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

$netSimExe = Join-Path $root 'build\net_sim.exe'
if (-not (Test-Path $netSimExe)) {
    throw "no net_sim at $netSimExe; run ./scripts/build.ps1 first"
}

# Clean up any lingering processes
Get-Process xrEngine, net_sim -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 500

$hostCopy = Resolve-Copy $HostDir
$clientCopy = Resolve-Copy $ClientDir
$started = Get-Date

Write-Host "==> net_sim: relay port $RelayPort -> 127.0.0.1:$HostPort (delay ${DelayMs}ms, loss ${LossPercent}%)"
$relayProcess = Start-Process -FilePath $netSimExe `
    -ArgumentList "relay", "$RelayPort", "127.0.0.1:$HostPort", "$DelayMs", "$LossPercent" -PassThru

$hostArgs = @('-cs', '-nointro', '-nogameintro', '-force_flushlog', '-exec',
    "coop_listen $HostPort; coop_trace $TraceIntervalMs; start server($Server)")
$clientArgs = @('-cs', '-nointro', '-nogameintro', '-force_flushlog', '-exec',
    "coop_connect 127.0.0.1:$RelayPort $ClientName; coop_trace $TraceIntervalMs; coop_drive $DriveScript; start server($Server)")

Write-Host "==> host:   $($hostCopy.Exe) (listen $HostPort)"
$hostProcess = Start-Process -FilePath $hostCopy.Exe -WorkingDirectory $hostCopy.Root `
    -ArgumentList $hostArgs -PassThru
$clientProcess = $null

$firstPos = $null
$lastPos = $null
$maxDistance = 0.0
$sawCrouch = $false
$sawJump = $false
$sawSprint = $false

try {
    Start-Sleep -Seconds $ClientDelaySeconds
    Write-Host "==> client: $($clientCopy.Exe) (connect relay 127.0.0.1:$RelayPort, drive: $DriveScript)"
    $clientProcess = Start-Process -FilePath $clientCopy.Exe -WorkingDirectory $clientCopy.Root `
        -ArgumentList $clientArgs -PassThru

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    $hostLog = $null
    $clientLog = $null
    $checks = [ordered]@{}

    while ((Get-Date) -lt $deadline) {
        $hostLog = Newest-Log $hostCopy.Logs $started
        $clientLog = Newest-Log $clientCopy.Logs $started

        if ($hostLog) {
            $checks['host listening'] = Test-Pattern $hostLog.FullName 'coop: hosting on port'
            $checks['host sees peer'] = Test-Pattern $hostLog.FullName "coop: peer \d+ \('$ClientName'\) joined"
            $checks['host doll'] = Test-Pattern $hostLog.FullName 'coop: remote player \d+ spawned as game id \d+'

            # Parse trace doll lines in host log:
            # * coop: trace doll <netId> (game id <id>) at <x> <y> <z>, yaw <yaw>, mstate 0x<hex>
            $traceMatches = Select-String -Path $hostLog.FullName -Pattern 'coop: trace doll \d+ .* at ([-0-9.]+) ([-0-9.]+) ([-0-9.]+), yaw ([-0-9.]+), mstate 0x([0-9a-fA-F]+)'
            foreach ($match in $traceMatches) {
                $g = $match.Matches[0].Groups
                $x = [double]::Parse($g[1].Value, [System.Globalization.CultureInfo]::InvariantCulture)
                $y = [double]::Parse($g[2].Value, [System.Globalization.CultureInfo]::InvariantCulture)
                $z = [double]::Parse($g[3].Value, [System.Globalization.CultureInfo]::InvariantCulture)
                $mstate = [Convert]::ToUInt32($g[5].Value, 16)

                if ($null -eq $firstPos) {
                    $firstPos = @{ X = $x; Y = $y; Z = $z }
                }
                $lastPos = @{ X = $x; Y = $y; Z = $z }

                $dist = [Math]::Sqrt([Math]::Pow($x - $firstPos.X, 2) + [Math]::Pow($z - $firstPos.Z, 2))
                if ($dist -gt $maxDistance) { $maxDistance = $dist }

                # mcCrouch = 0x0010, mcJump = 0x0080, mcSprint = 0x1000
                if (($mstate -band 0x0010) -ne 0) { $sawCrouch = $true }
                if (($mstate -band 0x0080) -ne 0) { $sawJump = $true }
                if (($mstate -band 0x1000) -ne 0) { $sawSprint = $true }
            }
        }

        if ($clientLog) {
            $checks['client joined'] = Test-Pattern $clientLog.FullName 'coop: joined '
            $checks['client doll'] = Test-Pattern $clientLog.FullName 'coop: remote player \d+ spawned as game id \d+'
            $checks['client drive'] = Test-Pattern $clientLog.FullName 'coop: debug drive started'
        }

        $checks['doll moved'] = $maxDistance -ge 1.5
        $checks['doll crouched'] = $sawCrouch
        $checks['doll jumped'] = $sawJump
        $checks['doll sprinted'] = $sawSprint

        # Check if all required criteria have passed
        $allPassed = $checks.Count -ge 10 -and -not ($checks.Values -contains $false)
        if ($allPassed) { break }

        Start-Sleep -Milliseconds 500
    }
}
finally {
    if (-not $Keep) {
        foreach ($p in @($hostProcess, $clientProcess, $relayProcess)) {
            if ($p -and -not $p.HasExited) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }
        }
    }
}

Write-Host ''
Write-Host "host log:   $(if ($hostLog) { $hostLog.FullName } else { 'none' })"
Write-Host "client log: $(if ($clientLog) { $clientLog.FullName } else { 'none' })"
Write-Host ''
Write-Host "==> Motion statistics:"
Write-Host ("  Displacement: {0:F2} m (target: >= 1.5 m)" -f $maxDistance)
Write-Host ("  Crouch seen:  {0}" -f $sawCrouch)
Write-Host ("  Jump seen:    {0}" -f $sawJump)
Write-Host ("  Sprint seen:  {0}" -f $sawSprint)
Write-Host ''

$order = 'host listening', 'host sees peer', 'client joined', 'client drive', 'host doll', 'client doll', 'doll moved', 'doll crouched', 'doll jumped', 'doll sprinted'
foreach ($key in $order) {
    $ok = $checks.Contains($key) -and [bool]$checks[$key]
    Write-Host ("{0,-16} {1}" -f $key, $(if ($ok) { 'ok' } else { 'FAILED' }))
}

if ($ShowLogs) {
    foreach ($log in @($hostLog, $clientLog)) {
        if (-not $log) { continue }
        Write-Host ''
        Write-Host "--- $($log.FullName)"
        Select-String -Path $log.FullName -Pattern 'coop:|exec:|! coop' |
            Select-Object -Last 35 | ForEach-Object { $_.Line }
    }
}

$ok = -not ($order | Where-Object { -not ($checks.Contains($_) -and [bool]$checks[$_]) })
if ($ok) { Write-Host ''; Write-Host 'ALL MOTION CHECKS PASSED (v0.1 green)' } else { Write-Host ''; Write-Host 'FAILED' }
exit $(if ($ok) { 0 } else { 1 })
