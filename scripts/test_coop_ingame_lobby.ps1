# Live 2-player test of the In-Game "Ready/Not Ready" Lobby with Skin Settings.
#
# Verifies on real running OpenXRay Host + Client:
# 1. In-game UICoopLobby opens with "Ready" button and saved-game button rule:
#    - With saved game present: ONLY "Continue" is shown ("New Game" hidden).
# 2. Match start is blocked while players are Not Ready.
# 3. In-game skin selection syncs over COOP_INFO and resets Ready if changed.
# 4. Only after BOTH Host and Client click "Ready" does Host start the match.
# 5. Also verifies "No Saved Game" rule (shows BOTH "Continue" and "New Game").

param(
    [string]$HostDir = 'local\game-host',
    [string]$ClientDir = 'local\game-client',
    [int]$Port = 27016,
    [string]$Server = 'all/single/alife/new',
    [int]$ClientDelaySeconds = 12,
    [int]$TimeoutSeconds = 60,
    [string]$ClientName = 'Tester',
    [switch]$ShowLogs
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

function Resolve-Copy([string]$path) {
    $full = if ([System.IO.Path]::IsPathRooted($path)) { $path } else { Join-Path $root $path }
    $exe = Join-Path $full 'bin\x64\xrEngine.exe'
    if (-not (Test-Path $exe)) { throw "no engine at $exe" }
    return [pscustomobject]@{
        Root = $full
        Exe = $exe
        AppData = (Join-Path $full '_appdata_')
        Logs = (Join-Path $full '_appdata_\logs')
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

function Send-LobbyCmd([object]$copy, [string]$commands) {
    $cmdPath = Join-Path $copy.AppData 'coop_lobby_cmd.txt'
    Set-Content -Path $cmdPath -Value $commands -Encoding ASCII
}

Get-Process xrEngine -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 500

$hostCopy = Resolve-Copy $HostDir
$clientCopy = Resolve-Copy $ClientDir

# Ensure fresh gamedata scripts are copied
Copy-Item (Join-Path $root 'gamedata\*') (Join-Path $hostCopy.Root 'gamedata') -Recurse -Force
Copy-Item (Join-Path $root 'gamedata\*') (Join-Path $clientCopy.Root 'gamedata') -Recurse -Force

$checks = [ordered]@{}

# Ensure Run 1 has at least one .sav file
$run1Saves = Get-ChildItem $hostCopy.Saves -Filter '*.sav' -File -ErrorAction SilentlyContinue
if (-not $run1Saves -or $run1Saves.Count -eq 0) {
    throw "Run 1 requires at least one .sav file in $($hostCopy.Saves)"
}

# --- RUN 1: Host WITH saved game -> Continue-only visibility + Ready gate + Match Start ---
$run1HostTrace = @()
$run1ClientTrace = @()
if (Test-Path $hostCopy.Logs) { Get-ChildItem $hostCopy.Logs -Filter '*.log' | Remove-Item -Force -ErrorAction SilentlyContinue }
if (Test-Path $clientCopy.Logs) { Get-ChildItem $clientCopy.Logs -Filter '*.log' | Remove-Item -Force -ErrorAction SilentlyContinue }
Remove-Item (Join-Path $hostCopy.AppData 'coop_lobby_cmd.txt') -Force -ErrorAction SilentlyContinue
Remove-Item (Join-Path $clientCopy.AppData 'coop_lobby_cmd.txt') -Force -ErrorAction SilentlyContinue

Set-Content -Path (Join-Path $hostCopy.AppData 'coop_lobby_mode') -Value '1' -Encoding ASCII
Set-Content -Path (Join-Path $clientCopy.AppData 'coop_lobby_mode') -Value '1' -Encoding ASCII

$started = Get-Date
$hostArgs = @('-cs', '-nointro', '-nogameintro', '-force_flushlog', '-exec', "coop_listen $Port; start server($Server)")
$clientArgs = @('-cs', '-nointro', '-nogameintro', '-force_flushlog', '-exec', "coop_connect 127.0.0.1:$Port $ClientName; start server($Server)")

Write-Host "==> [Run 1: With Save] Starting Host and Client into In-Game Lobby..."
$hostProcess = Start-Process -FilePath $hostCopy.Exe -WorkingDirectory $hostCopy.Root -ArgumentList $hostArgs -PassThru
$clientProcess = $null

try {
    Start-Sleep -Seconds $ClientDelaySeconds
    $clientProcess = Start-Process -FilePath $clientCopy.Exe -WorkingDirectory $clientCopy.Root -ArgumentList $clientArgs -PassThru

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    $hostLog = $null
    $clientLog = $null

    while ((Get-Date) -lt $deadline) {
        $hostLog = Newest-Log $hostCopy.Logs $started
        $clientLog = Newest-Log $clientCopy.Logs $started
        if ($hostLog -and $clientLog) {
            $hOpen = Test-Pattern $hostLog.FullName '\[coop_lobby\] In-game UICoopLobby opened'
            $cOpen = Test-Pattern $clientLog.FullName '\[coop_lobby\] In-game UICoopLobby opened'
            $cJoin = Test-Pattern $clientLog.FullName 'coop: joined '
            if ($hOpen -and $cOpen -and $cJoin) { break }
        }
        Start-Sleep -Milliseconds 500
    }

    $checks['host lobby opened'] = $hostLog -and (Test-Pattern $hostLog.FullName '\[coop_lobby\] In-game UICoopLobby opened')
    $checks['client lobby opened'] = $clientLog -and (Test-Pattern $clientLog.FullName '\[coop_lobby\] In-game UICoopLobby opened')
    $checks['save exists -> Continue ONLY shown'] = $hostLog -and (Test-Pattern $hostLog.FullName 'has_saved_game=true, Continue=true, NewGame=false, Ready=true')

    # Verify Host cannot start before both select skin & Ready
    Send-LobbyCmd $hostCopy "continue"
    Start-Sleep -Seconds 2
    $checks['start blocked before Ready'] = $hostLog -and (Test-Pattern $hostLog.FullName '\[coop_lobby\] Blocked Continue: not all players are Ready')

    # Select skins in-game and test Ready reset on skin change
    Send-LobbyCmd $hostCopy "skin cs_2`nready 1"
    Send-LobbyCmd $clientCopy "skin cs_3`nready 1"
    Start-Sleep -Seconds 2

    # Client switches skin -> must reset Ready=false, so Continue must still be blocked
    Send-LobbyCmd $clientCopy "skin cs_seva"
    Start-Sleep -Seconds 2
    Send-LobbyCmd $hostCopy "continue"
    Start-Sleep -Seconds 2
    $checks['skin change resets Ready & blocks start'] = $hostLog -and ((Select-String -Path $hostLog.FullName -Pattern '\[coop_lobby\] Blocked Continue: not all players are Ready').Count -ge 2)

    # Client now clicks Ready -> Host clicks Continue -> both enter match!
    Send-LobbyCmd $clientCopy "ready 1"
    Start-Sleep -Seconds 2
    Send-LobbyCmd $hostCopy "continue"

    $startDeadline = (Get-Date).AddSeconds(30)
    while ((Get-Date) -lt $startDeadline) {
        if ((Test-Pattern $hostLog.FullName '\[coop_lobby\] All players Ready -> Host starting Continue') -and
            (Test-Pattern $clientLog.FullName '\[coop_lobby\] Client received Host start signal')) {
            break
        }
        Start-Sleep -Milliseconds 500
    }

    $checks['host started Continue after all Ready'] = $hostLog -and (Test-Pattern $hostLog.FullName '\[coop_lobby\] All players Ready -> Host starting Continue')
    $checks['client entered match on start signal'] = $clientLog -and (Test-Pattern $clientLog.FullName '\[coop_lobby\] Client received Host start signal')

    if ($hostLog -and (Test-Path $hostLog.FullName)) {
        $run1HostTrace = @(Select-String -Path $hostLog.FullName -Pattern '\[coop_lobby\]|coop:' | Select-Object -Last 35 | ForEach-Object { $_.Line })
    }
    if ($clientLog -and (Test-Path $clientLog.FullName)) {
        $run1ClientTrace = @(Select-String -Path $clientLog.FullName -Pattern '\[coop_lobby\]|coop:' | Select-Object -Last 25 | ForEach-Object { $_.Line })
    }
}
finally {
    foreach ($p in @($hostProcess, $clientProcess)) {
        if ($p -and -not $p.HasExited) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }
    }
}

# --- RUN 2: Host WITHOUT saved games -> BOTH Continue and New Game shown ---
Start-Sleep -Milliseconds 500
$backupSavesDir = Join-Path $hostCopy.AppData 'savedgames_backup_test'
if (Test-Path $backupSavesDir) { Remove-Item $backupSavesDir -Recurse -Force }
if (Test-Path $hostCopy.Saves) {
    Move-Item $hostCopy.Saves $backupSavesDir -Force
}
New-Item -ItemType Directory -Path $hostCopy.Saves -Force | Out-Null

if (Test-Path $hostCopy.Logs) { Get-ChildItem $hostCopy.Logs -Filter '*.log' | Remove-Item -Force -ErrorAction SilentlyContinue }
if (Test-Path $clientCopy.Logs) { Get-ChildItem $clientCopy.Logs -Filter '*.log' | Remove-Item -Force -ErrorAction SilentlyContinue }
Remove-Item (Join-Path $hostCopy.AppData 'coop_lobby_cmd.txt') -Force -ErrorAction SilentlyContinue
Remove-Item (Join-Path $clientCopy.AppData 'coop_lobby_cmd.txt') -Force -ErrorAction SilentlyContinue
Set-Content -Path (Join-Path $hostCopy.AppData 'coop_lobby_mode') -Value '1' -Encoding ASCII
Set-Content -Path (Join-Path $clientCopy.AppData 'coop_lobby_mode') -Value '1' -Encoding ASCII

$started2 = Get-Date
Write-Host "==> [Run 2: No Saves] Verifying BOTH Continue and New Game are shown + New Game start..."
$hostProcess2 = Start-Process -FilePath $hostCopy.Exe -WorkingDirectory $hostCopy.Root -ArgumentList $hostArgs -PassThru
$clientProcess2 = $null

try {
    Start-Sleep -Seconds $ClientDelaySeconds
    $clientProcess2 = Start-Process -FilePath $clientCopy.Exe -WorkingDirectory $clientCopy.Root -ArgumentList $clientArgs -PassThru

    $deadline2 = (Get-Date).AddSeconds($TimeoutSeconds)
    $hostLog2 = $null
    $clientLog2 = $null
    while ((Get-Date) -lt $deadline2) {
        $hostLog2 = Newest-Log $hostCopy.Logs $started2
        $clientLog2 = Newest-Log $clientCopy.Logs $started2
        if ($hostLog2 -and $clientLog2) {
            if ((Test-Pattern $hostLog2.FullName '\[coop_lobby\] In-game UICoopLobby opened') -and
                (Test-Pattern $clientLog2.FullName '\[coop_lobby\] In-game UICoopLobby opened') -and
                (Test-Pattern $clientLog2.FullName 'coop: joined ')) {
                break
            }
        }
        Start-Sleep -Milliseconds 500
    }

    $checks['no save -> Continue AND New Game shown'] = $hostLog2 -and (Test-Pattern $hostLog2.FullName 'has_saved_game=false, Continue=true, NewGame=true, Ready=true')

    Send-LobbyCmd $hostCopy "skin cs_1`nready 1"
    Send-LobbyCmd $clientCopy "skin cs_2`nready 1"
    Start-Sleep -Seconds 2
    Send-LobbyCmd $hostCopy "new_game"

    $startDeadline2 = (Get-Date).AddSeconds(20)
    while ((Get-Date) -lt $startDeadline2) {
        if ((Test-Pattern $hostLog2.FullName '\[coop_lobby\] All players Ready -> Host starting New Game') -and
            (Test-Pattern $clientLog2.FullName '\[coop_lobby\] Client received Host start signal')) {
            break
        }
        Start-Sleep -Milliseconds 500
    }

    $checks['host started New Game after all Ready'] = $hostLog2 -and (Test-Pattern $hostLog2.FullName '\[coop_lobby\] All players Ready -> Host starting New Game')
}
finally {
    foreach ($p in @($hostProcess2, $clientProcess2)) {
        if ($p -and -not $p.HasExited) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }
    }
    if (Test-Path $backupSavesDir) {
        Remove-Item $hostCopy.Saves -Recurse -Force -ErrorAction SilentlyContinue
        Move-Item $backupSavesDir $hostCopy.Saves -Force
    }
}

Write-Host ''
Write-Host '==> In-Game Lobby Verification Results:'
foreach ($key in $checks.Keys) {
    $ok = [bool]$checks[$key]
    Write-Host ("  {0,-42} {1}" -f $key, $(if ($ok) { 'ok' } else { 'FAILED' }))
}

if ($ShowLogs) {
    if ($run1HostTrace.Count -gt 0) {
        Write-Host ''
        Write-Host '--- Run 1 Host Lobby Log'
        $run1HostTrace | ForEach-Object { $_ }
    }
    if ($hostLog2 -and (Test-Path $hostLog2.FullName)) {
        Write-Host ''
        Write-Host "--- Run 2 Host Lobby Log ($($hostLog2.FullName))"
        Select-String -Path $hostLog2.FullName -Pattern '\[coop_lobby\]|coop:' | Select-Object -Last 25 | ForEach-Object { $_.Line }
    }
}

$allOk = -not ($checks.Values -contains $false) -and ($checks.Count -ge 9)
if ($allOk) {
    Write-Host ''
    Write-Host 'ALL IN-GAME LOBBY & SKIN SELECTION CHECKS PASSED'
    exit 0
} else {
    Write-Host ''
    Write-Host 'FAILED'
    exit 1
}
