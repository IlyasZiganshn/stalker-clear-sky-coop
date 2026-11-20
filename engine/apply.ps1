# Applies our engine patches on top of the pinned upstream checkout.
# Order: copy engine/patches/files into src/, copy the coopnet library into
# xrNetServer/, then apply engine/patches/*.patch in lexicographic order.
# Refuses to continue when the checkout is not the commit from upstream.txt.

param(
    [string]$Destination = (Join-Path $PSScriptRoot 'src')
)

$ErrorActionPreference = 'Stop'

$upstream = Join-Path $PSScriptRoot 'upstream.txt'
if (!(Test-Path $upstream)) {
    throw "engine: upstream.txt not found at $upstream"
}

$values = @{}
foreach ($line in Get-Content $upstream) {
    $trimmed = $line.Trim()
    if ($trimmed -eq '' -or $trimmed.StartsWith('#')) { continue }
    $split = $trimmed.IndexOf('=')
    if ($split -le 0) { continue }
    $values[$trimmed.Substring(0, $split)] = $trimmed.Substring($split + 1)
}

$commit = $values['commit']
if (!$commit) {
    throw "engine: upstream.txt must contain commit="
}

if (!(Test-Path (Join-Path $Destination '.git'))) {
    throw "engine: $Destination is not a checkout; run engine/fetch.ps1 first"
}

$head = (git -C $Destination rev-parse HEAD).Trim()
if ($head -ne $commit) {
    throw "engine: expected $commit, got $head; run engine/fetch.ps1 to reset"
}

$sourceRoot = Join-Path $Destination 'src'

$filesRoot = Join-Path $PSScriptRoot 'patches\files'
if (Test-Path $filesRoot) {
    Copy-Item (Join-Path $filesRoot '*') $sourceRoot -Recurse -Force
}

# coopnet is engine-agnostic and lives outside the fork; the build compiles it
# as part of xrNetServer.
$coopnet = Join-Path (Split-Path -Parent $PSScriptRoot) 'coopnet'
$coopnetTarget = Join-Path $sourceRoot 'xrNetServer\coopnet'
New-Item -ItemType Directory -Force -Path $coopnetTarget | Out-Null
foreach ($part in 'include', 'src') {
    $from = Join-Path $coopnet $part
    if (!(Test-Path $from)) {
        throw "engine: coopnet/$part not found at $from"
    }
    Copy-Item $from $coopnetTarget -Recurse -Force
}

$patches = @(Get-ChildItem (Join-Path $PSScriptRoot 'patches') -Filter '*.patch' | Sort-Object Name)

# git writes patch diagnostics to stderr; in PowerShell that must not abort the
# script, so native calls run with Continue and report through exit codes.
function Invoke-GitApply([string[]]$Arguments) {
    $ErrorActionPreference = 'Continue'
    $output = & git -C $Destination @Arguments 2>&1
    $script:gitOutput = ($output | Out-String).Trim()
    return $LASTEXITCODE
}

foreach ($patch in $patches) {
    if ((Invoke-GitApply @('apply', '--reverse', '--check', $patch.FullName)) -eq 0) {
        Write-Host "==> $($patch.Name): already applied"
        continue
    }

    if ((Invoke-GitApply @('apply', '--check', $patch.FullName)) -ne 0) {
        throw "engine: $($patch.Name) does not apply; the checkout is not clean`n$gitOutput"
    }

    if ((Invoke-GitApply @('apply', $patch.FullName)) -ne 0) {
        throw "engine: cannot apply $($patch.Name)`n$gitOutput"
    }
    Write-Host "==> $($patch.Name): applied"
}

Write-Host "engine: patched $($patches.Count) file(s), coopnet copied to src/xrNetServer/coopnet"