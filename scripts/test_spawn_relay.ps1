# A spawn client through net_sim: the host's spawn blob and the client's own
# blob cross a lossy relay on the reliable control channel. A late joiner must
# get every spawn the host already knows about.

param(
    [string]$BuildDir = "$PSScriptRoot\..\build",
    [int]$DelayMs = 40,
    [int]$LossPercent = 2,
    [int]$RunMs = 4000,
    [int]$ServerPort = 27186,
    [int]$RelayPort = 27187
)

$ErrorActionPreference = 'Stop'

$exe = Join-Path $BuildDir 'coopnet_spawn.exe'
$sim = Join-Path $BuildDir 'net_sim.exe'
foreach ($path in @($exe, $sim)) {
    if (-not (Test-Path $path)) {
        Write-Host "spawn relay test: missing $path"
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
    $processes += Start-Redirected $sim @('relay', "$RelayPort", "127.0.0.1:$ServerPort", "$DelayMs", "$LossPercent", '15000')
    Start-Sleep -Milliseconds 500

    $client = Start-Redirected $exe @('client', "$RelayPort", 'alpha', "$RunMs")
    $processes += $client

    $null = $client.WaitForExit(20000)

    $alpha = $client.StandardOutput.ReadToEnd()
    Write-Host "--- alpha ---"
    Write-Host $alpha

    $exit = if ($client.HasExited) { $client.ExitCode } else { -1 }
    $ok = $exit -eq 0 -and
        $alpha -match 'net 1 blob \d+ bytes' -and
        $alpha -match 'malformed 0' -and
        $alpha -match 'sent 1'
    if ($ok) {
        exit 0
    }
    Write-Host "spawn relay test: FAILED (alpha exit $exit)"
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