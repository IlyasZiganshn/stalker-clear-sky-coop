# Runs the snapshot demo through net_sim (80 ms RTT, 2% loss by default) and
# fails if delta application breaks down under those conditions.
param(
    [string]$BuildDir = "$PSScriptRoot\..\build",
    [int]$DelayMs = 40,
    [int]$LossPercent = 2,
    [int]$RunMs = 5000,
    [int]$ServerPort = 27126,
    [int]$RelayPort = 27127
)

$ErrorActionPreference = 'Stop'

$exe = Join-Path $BuildDir 'coopnet_snap.exe'
$sim = Join-Path $BuildDir 'net_sim.exe'
foreach ($path in @($exe, $sim)) {
    if (-not (Test-Path $path)) {
        Write-Host "relay test: missing $path"
        exit 1
    }
}

$logDir = Join-Path ([System.IO.Path]::GetTempPath()) 'clear_sky_coop_relay'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$serverLog = Join-Path $logDir 'server.txt'
$relayLog = Join-Path $logDir 'relay.txt'
Remove-Item $serverLog, $relayLog -ErrorAction SilentlyContinue

$server = $null
$relay = $null
$code = 1

try {
    $server = Start-Process -FilePath $exe -ArgumentList 'server', "$ServerPort" `
        -RedirectStandardOutput $serverLog -PassThru -WindowStyle Hidden
    Start-Sleep -Milliseconds 500
    $relay = Start-Process -FilePath $sim `
        -ArgumentList 'relay', "$RelayPort", "127.0.0.1:$ServerPort", "$DelayMs", "$LossPercent", '15000' `
        -RedirectStandardOutput $relayLog -PassThru -WindowStyle Hidden
    Start-Sleep -Milliseconds 500

    $output = & $exe client "$RelayPort" "$RunMs"
    $clientCode = $LASTEXITCODE
    $output | ForEach-Object { Write-Host $_ }

    $text = $output -join "`n"
    if ($clientCode -eq 0 -and $text -match 'rejected (\d+)') {
        if ([int]$Matches[1] -eq 0) {
            $code = 0
        }
    }
    if ($code -ne 0) {
        Write-Host "relay test: FAILED (client exit $clientCode)"
    }
}
finally {
    foreach ($process in @($relay, $server)) {
        if ($null -ne $process -and -not $process.HasExited) {
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        }
    }
}

exit $code