# Object ID consistency verification test:
# Boots host and client with OpenXRay, queues coop_dump on level load,
# extracts dumped level entities from both engine logs, and compares whether
# game IDs, sections, and positions match between host and client.
#
# Usage:
#   ./scripts/test_coop_dump.ps1
#   ./scripts/test_coop_dump.ps1 -Limit 50 -ShowMatches

param(
    [string]$HostDir = 'local\game-host',
    [string]$ClientDir = 'local\game-client',
    [int]$Port = 27016,
    [string]$Server = 'all/single/alife/new',
    [int]$Limit = 100,
    [int]$ClientDelaySeconds = 15,
    [int]$TimeoutSeconds = 180,
    [string]$ClientName = 'Tester',
    [switch]$ShowMatches,
    [switch]$Keep
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

Get-Process xrEngine -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 500

$hostCopy = Resolve-Copy $HostDir
$clientCopy = Resolve-Copy $ClientDir
$started = Get-Date

$hostArgs = @('-cs', '-nointro', '-nogameintro', '-force_flushlog', '-exec',
    "coop_listen $Port; coop_dump $Limit; start server($Server)")
$clientArgs = @('-cs', '-nointro', '-nogameintro', '-force_flushlog', '-exec',
    "coop_connect 127.0.0.1:$Port $ClientName; coop_dump $Limit; start server($Server)")

Write-Host "==> host:   $($hostCopy.Exe) (listen $Port, dump $Limit)"
$hostProcess = Start-Process -FilePath $hostCopy.Exe -WorkingDirectory $hostCopy.Root `
    -ArgumentList $hostArgs -PassThru
$clientProcess = $null

try {
    Start-Sleep -Seconds $ClientDelaySeconds
    Write-Host "==> client: $($clientCopy.Exe) (connect 127.0.0.1:$Port, dump $Limit)"
    $clientProcess = Start-Process -FilePath $clientCopy.Exe -WorkingDirectory $clientCopy.Root `
        -ArgumentList $clientArgs -PassThru

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    $hostLog = $null
    $clientLog = $null
    $hostDumped = $false
    $clientDumped = $false

    while ((Get-Date) -lt $deadline) {
        $hostLog = Newest-Log $hostCopy.Logs $started
        $clientLog = Newest-Log $clientCopy.Logs $started

        if ($hostLog -and -not $hostDumped) {
            $hostDumped = Select-String -Path $hostLog.FullName -Pattern 'coop: \d+ objects in level, dumping' -Quiet
        }
        if ($clientLog -and -not $clientDumped) {
            $clientDumped = Select-String -Path $clientLog.FullName -Pattern 'coop: \d+ objects in level, dumping' -Quiet
        }

        if ($hostDumped -and $clientDumped) { break }
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
Write-Host ''

if (-not $hostDumped -or -not $clientDumped) {
    Write-Host ("FAILED: dump not found (host dumped: {0}, client dumped: {1})" -f $hostDumped, $clientDumped)
    exit 1
}

function Parse-Dump($logPath) {
    $objects = @{}
    $lines = Select-String -Path $logPath -Pattern 'coop: obj (\d+) (\S+) at ([-0-9.]+) ([-0-9.]+) ([-0-9.]+)'
    foreach ($line in $lines) {
        $g = $line.Matches[0].Groups
        $id = [int]$g[1].Value
        $sec = $g[2].Value
        $x = [double]::Parse($g[3].Value, [System.Globalization.CultureInfo]::InvariantCulture)
        $y = [double]::Parse($g[4].Value, [System.Globalization.CultureInfo]::InvariantCulture)
        $z = [double]::Parse($g[5].Value, [System.Globalization.CultureInfo]::InvariantCulture)
        $objects[$id] = @{ Id = $id; Section = $sec; X = $x; Y = $y; Z = $z }
    }
    return $objects
}

$hostObjs = Parse-Dump $hostLog.FullName
$clientObjs = Parse-Dump $clientLog.FullName

Write-Host ("Host objects parsed:   {0}" -f $hostObjs.Count)
Write-Host ("Client objects parsed: {0}" -f $clientObjs.Count)
Write-Host ''

$matchingIds = 0
$matchingSecs = 0
$matchingPos = 0

foreach ($id in $hostObjs.Keys) {
    if ($clientObjs.ContainsKey($id)) {
        $matchingIds++
        $h = $hostObjs[$id]
        $c = $clientObjs[$id]
        if ($h.Section -eq $c.Section) { $matchingSecs++ }
        $dist = [Math]::Sqrt([Math]::Pow($h.X - $c.X, 2) + [Math]::Pow($h.Y - $c.Y, 2) + [Math]::Pow($h.Z - $c.Z, 2))
        if ($dist -lt 0.05) { $matchingPos++ }

        if ($ShowMatches) {
            Write-Host ("  ID {0,-5}: {1,-25} dist: {2:F3}m" -f $id, $h.Section, $dist)
        }
    }
}

Write-Host "Consistency analysis:"
Write-Host ("  Total compared:    {0}" -f $hostObjs.Count)
Write-Host ("  Matching IDs:      {0} ({1:P1})" -f $matchingIds, ($matchingIds / [Math]::Max(1, $hostObjs.Count)))
Write-Host ("  Matching Sections: {0} ({1:P1})" -f $matchingSecs, ($matchingSecs / [Math]::Max(1, $matchingIds)))
Write-Host ("  Matching Position: {0} ({1:P1})" -f $matchingPos, ($matchingPos / [Math]::Max(1, $matchingIds)))
Write-Host ''

$ok = ($matchingIds -gt 0) -and ($matchingSecs -eq $matchingIds)
if ($ok) {
    Write-Host "OBJECT CONSISTENCY CHECK PASSED"
    exit 0
} else {
    Write-Host "OBJECT CONSISTENCY CHECK FAILED"
    exit 1
}
