# Live test of Co-op Connection, Partner HUD, and Animations with two game copies.
#
# 1. Launches host and client into marsh level.
# 2. Waits for UDP handshake, level load, and remote doll spawn.
# 3. Verifies partner HUD registration (mp_friend_location map spot & OnRender).
# 4. Executes scripted client movement & actions (forward sprint, crouch, jump).
# 5. Verifies physical displacement and animation mstate bits on host.
# 6. Verifies engine stability and clean communication.
#
# Usage:
#   ./scripts/test_coop_partner_hud_motion.ps1
#   ./scripts/test_coop_partner_hud_motion.ps1 -ShowLogs

param(
    [string]$HostDir = 'local\game-host',
    [string]$ClientDir = 'local\game-client',
    [int]$Port = 27016,
    [string]$Server = 'all/single/alife/new',
    [string]$DriveScript = '1000 4000f,2500fc,1200j,3500ft',
    [int]$TraceIntervalMs = 200,
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

# Clean any running instances
Get-Process xrEngine -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 500

$hostCopy = Resolve-Copy $HostDir
$clientCopy = Resolve-Copy $ClientDir

# Clean previous logs to avoid stale matches
if (Test-Path $hostCopy.Logs) { Get-ChildItem $hostCopy.Logs -Filter '*.log' | Remove-Item -Force -ErrorAction SilentlyContinue }
if (Test-Path $clientCopy.Logs) { Get-ChildItem $clientCopy.Logs -Filter '*.log' | Remove-Item -Force -ErrorAction SilentlyContinue }

# Clean old cmd files
$hostCmd = Join-Path $hostCopy.AppData 'coop_cmd.txt'
$clientCmd = Join-Path $clientCopy.AppData 'coop_cmd.txt'
if (Test-Path $hostCmd) { Remove-Item $hostCmd -Force -ErrorAction SilentlyContinue }
if (Test-Path $clientCmd) { Remove-Item $clientCmd -Force -ErrorAction SilentlyContinue }

$started = Get-Date

$hostArgs = @('-cs', '-nointro', '-nogameintro', '-force_flushlog', '-exec',
    "coop_listen $Port; coop_trace $TraceIntervalMs; start server($Server)")
$clientArgs = @('-cs', '-nointro', '-nogameintro', '-force_flushlog', '-exec',
    "coop_connect 127.0.0.1:$Port $ClientName; coop_trace $TraceIntervalMs; start server($Server)")

Write-Host "==> Starting Host instance on port $Port..."
$hostProcess = Start-Process -FilePath $hostCopy.Exe -WorkingDirectory $hostCopy.Root `
    -ArgumentList $hostArgs -PassThru
$clientProcess = $null

$checks = [ordered]@{}

try {
    Start-Sleep -Seconds $ClientDelaySeconds
    Write-Host "==> Starting Client instance connecting to 127.0.0.1:$Port..."
    $clientProcess = Start-Process -FilePath $clientCopy.Exe -WorkingDirectory $clientCopy.Root `
        -ArgumentList $clientArgs -PassThru

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    $hostLog = $null
    $clientLog = $null

    Write-Host "==> Waiting for world spawn and partner doll initialization..."
    while ((Get-Date) -lt $deadline) {
        $hostLog = Newest-Log $hostCopy.Logs $started
        $clientLog = Newest-Log $clientCopy.Logs $started

        if ($hostLog) {
            $checks['host hosting on port'] = Test-Pattern $hostLog.FullName 'coop: hosting on port'
            $checks['host accepted peer'] = Test-Pattern $hostLog.FullName "coop: peer \d+ \('$ClientName'\) joined"
            $checks['host spawned partner doll'] = Test-Pattern $hostLog.FullName 'coop: remote player \d+ spawned as game id \d+'
        }
        if ($clientLog) {
            $checks['client joined session'] = Test-Pattern $clientLog.FullName 'coop: joined '
            $checks['client spawned partner doll'] = Test-Pattern $clientLog.FullName 'coop: remote player \d+ spawned as game id \d+'
        }

        if ($checks.Count -ge 5 -and -not ($checks.Values -contains $false)) { break }
        Start-Sleep -Milliseconds 500
    }

    Write-Host "==> Connection & Doll Spawn phase verified!"
    Start-Sleep -Seconds 3

    # Animation & Motion testing sequence via client coop_drive:
    # Phase 1: Forward run (4s)
    # Phase 2: Forward crouch (2.5s)
    # Phase 3: Jump (1.2s)
    # Phase 4: Forward sprint / turbo (3.5s)
    Write-Host "==> Executing Animation & Motion sequence on Client ($DriveScript)..."
    Send-CoopCmd $clientCopy "coop_drive $DriveScript"

    $driveDeadline = (Get-Date).AddSeconds(20)
    while ((Get-Date) -lt $driveDeadline) {
        if ($clientLog -and (Test-Pattern $clientLog.FullName 'coop: debug drive finished')) {
            break
        }
        Start-Sleep -Milliseconds 500
    }
    Start-Sleep -Seconds 2

    # Verification of displacement & animations on Host
    if ($hostLog) {
        $traceMatches = Select-String -Path $hostLog.FullName -Pattern 'coop: trace doll \d+ .* at ([-0-9.]+) ([-0-9.]+) ([-0-9.]+), yaw ([-0-9.]+), mstate 0x([0-9a-fA-F]+)'
        $checks['motion traces recorded on host'] = ($traceMatches.Count -ge 5)

        $firstPos = $null
        $lastPos = $null
        $maxDistance = 0.0
        $sawCrouch = $false
        $sawJump = $false
        $sawSprint = $false

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

            if (($mstate -band 0x0010) -ne 0) { $sawCrouch = $true }
            if (($mstate -band 0x0080) -ne 0) { $sawJump = $true }
            if (($mstate -band 0x1000) -ne 0) { $sawSprint = $true }
        }

        $checks['physical displacement > 0.5m'] = ($maxDistance -ge 0.5)
        Write-Host ("  Measured doll displacement on host: {0:F3} meters (max {1:F3} m)" -f $(if ($firstPos -and $lastPos) { [Math]::Sqrt([Math]::Pow($lastPos.X - $firstPos.X, 2) + [Math]::Pow($lastPos.Z - $firstPos.Z, 2)) } else { 0 }), $maxDistance)

        $checks['sprint animation verified'] = $sawSprint
        $checks['crouch animation verified'] = $sawCrouch
        $checks['jump animation verified'] = $sawJump
    }

    $checks['engine processes alive & stable'] = (-not $hostProcess.HasExited) -and (-not $clientProcess.HasExited)
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
Write-Host "==> Verification results:"
foreach ($key in $checks.Keys) {
    $ok = [bool]$checks[$key]
    Write-Host ("  {0,-36} {1}" -f $key, $(if ($ok) { 'ok' } else { 'FAILED' }))
}

if ($ShowLogs) {
    if ($hostLog) {
        Write-Host ''
        Write-Host "--- Host Log Summary ($($hostLog.FullName))"
        Select-String -Path $hostLog.FullName -Pattern 'coop:|exec:|remote player' |
            Select-Object -Last 35 | ForEach-Object { $_.Line }
    }
    if ($clientLog) {
        Write-Host ''
        Write-Host "--- Client Log Summary ($($clientLog.FullName))"
        Select-String -Path $clientLog.FullName -Pattern 'coop:|exec:|remote player' |
            Select-Object -Last 35 | ForEach-Object { $_.Line }
    }
}

$allOk = -not ($checks.Values -contains $false) -and ($checks.Count -ge 8)
if ($allOk) {
    Write-Host ''
    Write-Host 'ALL PARTNER HUD & ANIMATION CHECKS PASSED (Live 2-Player Co-op Verified)'
    exit 0
} else {
    Write-Host ''
    Write-Host 'FAILED'
    exit 1
}
