# Fetches the pinned OpenXRay upstream into engine/src (git-ignored).
# Does not touch the game itself: the engine builds from source alone.

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

$repo = $values['repo']
$commit = $values['commit']
if (!$repo -or !$commit) {
    throw "engine: upstream.txt must contain repo= and commit="
}

$url = "https://github.com/$repo.git"

if (!(Test-Path (Join-Path $Destination '.git'))) {
    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    git -C $Destination init --quiet
    git -C $Destination remote add origin $url
}

git -C $Destination fetch --depth 1 origin $commit
if ($LASTEXITCODE -ne 0) {
    throw "engine: cannot fetch $commit from $url"
}

git -C $Destination checkout --detach --force FETCH_HEAD --quiet
if ($LASTEXITCODE -ne 0) {
    throw "engine: cannot check out $commit"
}

$head = (git -C $Destination rev-parse HEAD).Trim()
if ($head -ne $commit) {
    throw "engine: expected $commit, got $head"
}

Write-Host "engine: $repo @ $head ready in $Destination"