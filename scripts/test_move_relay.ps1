# Two moving clients through net_sim: each client has its own relay to the same
# server, so the server sees two endpoints. Fails if either client reports
# broken prediction, missing actors or rejected snapshots.
param(
    [string]$BuildDir = "$PSScriptRoot\..\build",
    [int]$DelayMs = 40,
    [int]$LossPercent = 2,
    [int]$RunMs = 6000,
    [int]$ServerPort = 27156,
    [int]$FirstRelayPort = 27157,
    [int]$SecondRelayPort = 27158
)

$ErrorActionPreference = 'Stop'

$exe = Join-Path $BuildDir 'coopnet_move.exe'
$sim = Join-Path $BuildDir 'net_sim.exe'
foreach ($path in @($exe, $sim)) {
    if (-not (Test-Path $path)) {
        Write-Host "move relay test: missing $path"
        exit 1
    }
}

# Start-Process with redirection leaves ExitCode empty on PowerShell 5.1, so
# processes are driven through .NET directly.
function Start-Redirected([string]$file, [string[]]$arguments) {
    $info = New-Object System.Diagnostics.ProcessStartInfo
    $info.FileName = $file
    $info.Arguments = ($arguments | ForEach-Object {
        if ($_ -match '\s') { '"' + $_ + '"' } else { $_ }
    }) -join ' '
    $info.UseShellExecute = $false
    $info.RedirectStandardOutput = $true
    $info.CreateNoWindow = $true
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $info
    $null = $process.Start()
    return $process
}

function Write-Output-Log($process, [string]$tag) {
    $text = $process.StandardOutput.ReadToEnd()
    foreach ($line in ($text -split "`r?`n" | Where-Object { $_ -ne '' })) {
        Write-Host "$tag$line"
    }
}

$processes = @()

try {
    $server = Start-Redirected $exe @('server', "$ServerPort")
    $processes += $server
    Start-Sleep -Milliseconds 500
    $relay1 = Start-Redirected $sim @('relay', "$FirstRelayPort", "127.0.0.1:$ServerPort", "$DelayMs", "$LossPercent", '15000')
    $processes += $relay1
    $relay2 = Start-Redirected $sim @('relay', "$SecondRelayPort", "127.0.0.1:$ServerPort", "$DelayMs", "$LossPercent", '15000')
    $processes += $relay2
    Start-Sleep -Milliseconds 500

    $client1 = Start-Redirected $exe @('client', "$FirstRelayPort", "$RunMs")
    $processes += $client1
    $client2 = Start-Redirected $exe @('client', "$SecondRelayPort", "$RunMs")
    $processes += $client2

    $null = $client1.WaitForExit(20000)
    $null = $client2.WaitForExit(20000)

    Write-Output-Log $client1 'client 1: '
    Write-Output-Log $client2 'client 2: '

    $exit1 = if ($client1.HasExited) { $client1.ExitCode } else { -1 }
    $exit2 = if ($client2.HasExited) { $client2.ExitCode } else { -1 }
    if ($exit1 -eq 0 -and $exit2 -eq 0) {
        exit 0
    }
    Write-Host "move relay test: FAILED (client 1 exit $exit1, client 2 exit $exit2)"
    exit 1
}
finally {
    foreach ($process in $processes) {
        try {
            if (-not $process.HasExited) {
                $process.Kill()
            }
        } catch {
        }
    }
}