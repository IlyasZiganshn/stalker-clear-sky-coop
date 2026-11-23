# package_release.ps1
# Prepares a complete, self-contained distribution package for the 2nd PC / player.

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$releaseDir = Join-Path $repoRoot 'release\ClearSkyCoop_Setup'
$zipPath = Join-Path $repoRoot 'release\ClearSkyCoop_Player_Setup.zip'

Write-Host "==> Creating release distribution at: $releaseDir"

if (Test-Path $releaseDir) {
    Remove-Item -Path $releaseDir -Recurse -Force
}
New-Item -ItemType Directory -Path $releaseDir -Force | Out-Null

# 1. Copy Launcher
$launcherExe = Join-Path $repoRoot 'ClearSkyCoopLauncher.exe'
if (-not (Test-Path $launcherExe)) {
    # Rebuild if not present
    & "$repoRoot\launcher\build.ps1"
}
Copy-Item -Path $launcherExe -Destination (Join-Path $releaseDir 'ClearSkyCoopLauncher.exe') -Force
Write-Host "  [+] Copied ClearSkyCoopLauncher.exe"

# 2. Copy bin\x64 binaries (excluding huge .pdb debug files)
$targetBin = Join-Path $releaseDir 'bin\x64'
New-Item -ItemType Directory -Path $targetBin -Force | Out-Null

$srcBin = Join-Path $repoRoot 'bin\x64'
Get-ChildItem -Path $srcBin -File | Where-Object { $_.Extension -ne '.pdb' } | ForEach-Object {
    Copy-Item -Path $_.FullName -Destination (Join-Path $targetBin $_.Name) -Force
}

# Also copy utils if any non-pdb
$srcUtils = Join-Path $srcBin 'utils'
if (Test-Path $srcUtils) {
    $targetUtils = Join-Path $targetBin 'utils'
    New-Item -ItemType Directory -Path $targetUtils -Force | Out-Null
    Get-ChildItem -Path $srcUtils -File | Where-Object { $_.Extension -ne '.pdb' } | ForEach-Object {
        Copy-Item -Path $_.FullName -Destination (Join-Path $targetUtils $_.Name) -Force
    }
}
Write-Host "  [+] Copied OpenXRay x64 runtime binaries and DLLs to bin\x64"

# 3. Copy gamedata
$srcGamedata = Join-Path $repoRoot 'gamedata'
if (Test-Path $srcGamedata) {
    $targetGamedata = Join-Path $releaseDir 'gamedata'
    Copy-Item -Path $srcGamedata -Destination $targetGamedata -Recurse -Force
    Write-Host "  [+] Copied gamedata scripts"
}

# 4. Create simple instruction file
$instruction = @"
=====================================================================
  S.T.A.L.K.E.R.: ЧИСТОЕ НЕБО - КООПЕРАТИВНЫЙ МОД (ДЛЯ НАПАРНИКА)
=====================================================================

ТРЕБОВАНИЯ:
- Установленная игра S.T.A.L.K.E.R.: Clear Sky (версия 1.5.10, Steam, GOG или диск).
- Подключение к хосту: одна локальная сеть (LAN) либо Radmin VPN / Hamachi / ZeroTier.

ИНСТРУКЦИЯ ПО УСТАНОВКЕ (2 минуты):
---------------------------------------------------------------------
1. Распакуйте эту папку в любое удобное место (например, на Рабочий стол).
2. Запустите ClearSkyCoopLauncher.exe.
3. Перейдите на вкладку 'Settings':
   - Лаунчер автоматически найдет путь к игре. Если игра установлена в нестандартную
     папку, нажмите кнопку 'Browse...' и укажите папку с установленной игрой
     (например, D:\Games\S.T.A.L.K.E.R. Clear Sky).
   - Нажмите синюю кнопку 'Install / Deploy Mod Files'.
   - Появится сообщение об успешной установке! Все нужные файлы и конфиги
     настроятся автоматически.

ПОДКЛЮЧЕНИЕ К СЕРВЕРУ ХОСТА:
---------------------------------------------------------------------
1. На вкладке 'Lobby / Join':
   - В поле 'Your Name' укажите свой игровой никнейм.
2. Подключение:
   - В локальной сети (LAN): через 1-2 секунды в списке серверов появится сервер
     напарника -> выберите его и нажмите 'Join Selected'.
   - Через Radmin VPN / Hamachi / Интернет: в поле 'Direct IP' введите IP-адрес хоста
     (который скопируете из Radmin VPN / Hamachi) и нажмите 'Connect Direct'.

3. Игра запустится автоматически и подключится к сессии!
=====================================================================
"@

Set-Content -Path (Join-Path $releaseDir 'ИНСТРУКЦИЯ.txt') -Value $instruction -Encoding utf8
Set-Content -Path (Join-Path $releaseDir 'INSTRUCTIONS.txt') -Value $instruction -Encoding utf8
Write-Host "  [+] Created ИНСТРУКЦИЯ.txt and INSTRUCTIONS.txt"

# 5. Create ZIP Archive
if (Test-Path $zipPath) {
    Remove-Item -Path $zipPath -Force
}

Write-Host "==> Compressing into $zipPath..."
Compress-Archive -Path "$releaseDir\*" -DestinationPath $zipPath -CompressionLevel Optimal

$zipSizeMb = [math]::Round((Get-Item $zipPath).Length / 1MB, 2)
Write-Host "==> DONE! Package ready: $zipPath ($zipSizeMb MB)"
