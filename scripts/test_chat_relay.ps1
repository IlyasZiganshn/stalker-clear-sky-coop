# Two chat clients through net_sim: each has its own relay to the same server.
# Reliable control messages must arrive despite 2% loss, so every client expects
# all five lines from the other one.
param(
    [string]$BuildDir = "$PSScriptRoot\..\build",
    [int]$DelayMs = 40,
    [int]$LossPercent = 2,
    [int]$RunMs = 4000,
    [int]$ServerPort = 27166,
    [int]$FirstRelayPort = 27167,
    [int]$SecondRelayPort = 27168
)

$ErrorActionPreference = 'Stop'

$exe = Join-Path $BuildDir 'coopnet_chat.exe'
$sim = Join-Path $BuildDir 'net_sim.exe'
foreach ($path in @($exe, $sim)) {
    if (-not (Test-Path $path)) {
        Write-Host "chat relay test: missing $path"
        exit 1
    }
}

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

$processes = @()
$outputs = @{}

try {
    $processes += Start-Redirected $exe @('server', "$ServerPort")
    Start-Sleep -Milliseconds 500
    $processes += Start-Redirected $sim @('relay', "$FirstRelayPort", "127.0.0.1:$ServerPort", "$DelayMs", "$LossPercent", '15000')
    $processes += Start-Redirected $sim @('relay', "$SecondRelayPort", "127.0.0.1:$ServerPort", "$DelayMs", "$LossPercent", '15000')
    Start-Sleep -Milliseconds 500

    $first = Start-Redirected $exe @('client', "$FirstRelayPort", 'alpha', "$RunMs")
    $second = Start-Redirected $exe @('client', "$SecondRelayPort", 'bravo', "$RunMs")
    $processes += $first
    $processes += $second

    $null = $first.WaitForExit(20000)
    $null = $second.WaitForExit(20000)

    $outputs.alpha = $first.StandardOutput.ReadToEnd()
    $outputs.bravo = $second.StandardOutput.ReadToEnd()
    Write-Host "--- alpha ---"
    Write-Host $outputs.alpha
    Write-Host "--- bravo ---"
    Write-Host $outputs.bravo

    $exit1 = if ($first.HasExited) { $first.ExitCode } else { -1 }
    $exit2 = if ($second.HasExited) { $second.ExitCode } else { -1 }
    $ok = $exit1 -eq 0 -and $exit2 -eq 0 -and
        $outputs.alpha -match 'received 5, malformed 0' -and
        $outputs.bravo -match 'received 5, malformed 0'
    if ($ok) {
        exit 0
    }
    Write-Host "chat relay test: FAILED (alpha exit $exit1, bravo exit $exit2)"
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