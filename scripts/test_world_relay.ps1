# Two world clients through net_sim: each has its own relay to the same server.
# Player transforms travel on the reliable control channel, so with 2% loss each
# client must still see the other's moving position.

param(
    [string]$BuildDir = "$PSScriptRoot\..\build",
    [int]$DelayMs = 40,
    [int]$LossPercent = 2,
    [int]$RunMs = 4000,
    [int]$ServerPort = 27176,
    [int]$FirstRelayPort = 27177,
    [int]$SecondRelayPort = 27178
)

$ErrorActionPreference = 'Stop'

$exe = Join-Path $BuildDir 'coopnet_world.exe'
$sim = Join-Path $BuildDir 'net_sim.exe'
foreach ($path in @($exe, $sim)) {
    if (-not (Test-Path $path)) {
        Write-Host "world relay test: missing $path"
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

    $alpha = $first.StandardOutput.ReadToEnd()
    $bravo = $second.StandardOutput.ReadToEnd()
    Write-Host "--- alpha ---"
    Write-Host $alpha
    Write-Host "--- bravo ---"
    Write-Host $bravo

    $exit1 = if ($first.HasExited) { $first.ExitCode } else { -1 }
    $exit2 = if ($second.HasExited) { $second.ExitCode } else { -1 }
    $ok = $exit1 -eq 0 -and $exit2 -eq 0 -and
        $alpha -match 'remote \d+ at [\d.]+ [\d.]+ [\d.]+, world l01_escape' -and
        $bravo -match 'remote \d+ at [\d.]+ [\d.]+ [\d.]+, world l01_escape' -and
        $alpha -notmatch 'received 0' -and
        $bravo -notmatch 'received 0'
    if ($ok) {
        exit 0
    }
    Write-Host "world relay test: FAILED (alpha exit $exit1, bravo exit $exit2)"
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