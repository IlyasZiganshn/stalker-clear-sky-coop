# test_mod_deployer.ps1
# Thorough automated verification of ModDeployer and installation edge cases

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$testGameDir = Join-Path $repoRoot 'local\test-install-sandbox'

Write-Host "==> Testing ModDeployer edge cases in sandbox: $testGameDir"

if (Test-Path $testGameDir) {
    # Clear read-only attributes before removing
    Get-ChildItem -Path $testGameDir -Recurse -Force -ErrorAction SilentlyContinue | ForEach-Object {
        $_.Attributes = 'Normal'
    }
    Remove-Item -Path $testGameDir -Recurse -Force
}

New-Item -ItemType Directory -Path $testGameDir -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $testGameDir 'bin') -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $testGameDir 'resources') -Force | Out-Null

# Create a realistic retail fsgame.ltx with tabs, short path and ReadOnly attribute
$mockFsgame = @"
;abbreviation           = recurs|notif|  root|                  add|        ext|            description
`$arch_dir`$              = false| false| `$fs_root`$
`$game_data`$             = false| true|  `$fs_root`$|            gamedata\
`$app_data_root`$`t`t= true|`tfalse|`tC:\Users\73B5~1\DOCUME~1\Stalker-STCS
`$logs`$                  = true|  false| `$app_data_root`$|      logs\
"@

$fsgamePath = Join-Path $testGameDir 'fsgame.ltx'
Set-Content -Path $fsgamePath -Value $mockFsgame -Encoding ASCII
# Set ReadOnly to test permission handling
(Get-Item $fsgamePath).Attributes = 'ReadOnly'

# Also create a dummy ReadOnly binary in bin\x64
$dummyBinDir = Join-Path $testGameDir 'bin\x64'
New-Item -ItemType Directory -Path $dummyBinDir -Force | Out-Null
$dummyExe = Join-Path $dummyBinDir 'xrEngine.exe'
Set-Content -Path $dummyExe -Value 'dummy'
(Get-Item $dummyExe).Attributes = 'ReadOnly'

Write-Host "--> Created mock retail game with ReadOnly fsgame.ltx and ReadOnly xrEngine.exe"

# Load Launcher assembly to test ModDeployer methods directly
$launcherDll = Join-Path $repoRoot 'bin\ClearSkyCoopLauncher.exe'
[System.Reflection.Assembly]::LoadFrom($launcherDll) | Out-Null

$modDeployerType = [System.Type]::GetType("ClearSkyCoop.Launcher.ModDeployer, ClearSkyCoopLauncher")
if (-not $modDeployerType) {
    # Try finding in loaded assemblies
    $modDeployerType = ([AppDomain]::CurrentDomain.GetAssemblies() | Where-Object { $_.GetName().Name -eq 'ClearSkyCoopLauncher' }).GetType("ClearSkyCoop.Launcher.ModDeployer")
}

if (-not $modDeployerType) {
    throw "Failed to load ClearSkyCoop.Launcher.ModDeployer type."
}

# 1. Test NormalizeGamePath with quotes and subfolders
$quotedPath = "`"$testGameDir`""
$norm1 = $modDeployerType::NormalizeGamePath($quotedPath)
if ($norm1 -ne $testGameDir) {
    throw "NormalizeGamePath failed on quoted path: expected $testGameDir, got $norm1"
}
Write-Host "  [PASS] Quoted path normalized"

$binSubPath = Join-Path $testGameDir 'bin'
$norm2 = $modDeployerType::NormalizeGamePath($binSubPath)
if ($norm2 -ne $testGameDir) {
    throw "NormalizeGamePath failed on subfolder 'bin': expected $testGameDir, got $norm2"
}
Write-Host "  [PASS] Subfolder 'bin' automatically resolved to game root"

# 2. Test DeployMod into sandbox
$deployMethod = $modDeployerType.GetMethod("DeployMod", [System.Reflection.BindingFlags]::Public -bor [System.Reflection.BindingFlags]::Static)
$methodArgs = [System.Object[]]::new(2)
$methodArgs[0] = [string]$testGameDir
$methodArgs[1] = [string]""

$success = [bool]$deployMethod.Invoke($null, $methodArgs)
if (-not $success) {
    $err = $methodArgs[1]
    throw "DeployMod failed: $err"
}
Write-Host "  [PASS] DeployMod completed successfully"

# 3. Check that xrEngine.exe was overwritten with real binary
$realExe = Join-Path $dummyBinDir 'xrEngine.exe'
$len = (Get-Item $realExe).Length
if ($len -lt 100000) {
    throw "xrEngine.exe was not overwritten with real binary, length: $len"
}
Write-Host "  [PASS] Overwrote ReadOnly xrEngine.exe with real OpenXRay binary (size: $len bytes)"

# 4. Check backup fsgame.ltx.coop_backup exists
$bak = Join-Path $testGameDir 'fsgame.ltx.coop_backup'
if (-not (Test-Path $bak)) {
    throw "fsgame.ltx.coop_backup was not created!"
}
Write-Host "  [PASS] Backup fsgame.ltx.coop_backup created"

# 5. Check patched fsgame.ltx
$patchedFsgame = Get-Content -Path $fsgamePath -Raw
if ($patchedFsgame -notmatch '\$app_data_root\$\s*=\s*true\|\s*false\|\s*\$fs_root\$\|\s*_appdata_\\') {
    throw "fsgame.ltx did not have $app_data_root$ updated to _appdata_! Content:`n$patchedFsgame"
}
if ($patchedFsgame -notmatch '\$game_data\$\s*=\s*true\|\s*true\|\s*\$fs_root\$\|\s*gamedata\\') {
    throw "fsgame.ltx did not have $game_data$ set to true| true! Content:`n$patchedFsgame"
}
Write-Host "  [PASS] fsgame.ltx correctly patched with _appdata_\ and loose gamedata"

# 6. Check _appdata_ directories and user.ltx
$logs = Join-Path $testGameDir '_appdata_\logs'
$saves = Join-Path $testGameDir '_appdata_\savedgames'
$userLtx = Join-Path $testGameDir '_appdata_\user.ltx'

if (-not (Test-Path $logs) -or -not (Test-Path $saves)) {
    throw "_appdata_ subdirectories were not created!"
}
if (-not (Test-Path $userLtx) -or ((Get-Content -Path $userLtx -Raw) -notmatch 'rs_always_active 1')) {
    throw "user.ltx missing rs_always_active 1"
}
Write-Host "  [PASS] _appdata_ subdirectories and user.ltx (rs_always_active 1) verified"

# 7. Check gamedata scripts deployed
$coopMain = Join-Path $testGameDir 'gamedata\scripts\coop_main.script'
if (-not (Test-Path $coopMain)) {
    throw "coop_main.script was not deployed to gamedata\scripts!"
}
Write-Host "  [PASS] gamedata scripts deployed successfully"

# 8. Check IsModInstalled
$installed = $modDeployerType::IsModInstalled($testGameDir)
if (-not $installed) {
    throw "IsModInstalled returned false on successfully deployed directory!"
}
Write-Host "  [PASS] IsModInstalled returns true"

# Clean up sandbox
Get-ChildItem -Path $testGameDir -Recurse -Force -ErrorAction SilentlyContinue | ForEach-Object {
    $_.Attributes = 'Normal'
}
Remove-Item -Path $testGameDir -Recurse -Force

Write-Host "==> ALL MOD DEPLOYER & INSTALLATION EDGE CASES VERIFIED SUCCESSFULLY!"
