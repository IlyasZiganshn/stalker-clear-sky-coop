# Форк движка

Здесь живут только наши изменения относительно OpenXRay. Сам движок в репозиторий
не кладём — он большой, и его история не нужна в истории мода.

## Модель

1. Апстрим фиксируется коммитом — он записан в `engine/upstream.txt` (сейчас `d1eb5d3`, 2026-11-16).
2. Наши изменения — патчи в `engine/patches/`, применяются в лексикографическом порядке.
3. Ветка форка на GitHub — опционально, для удобного ревью; источник правды всё равно
   патчи + зафиксированный коммит.

Исходники апстрима лежат в `src/` — пути ниже даны относительно него.

## Правила патчей

- Один патч — одна тема. Имя: `NNNN-краткое-имя.patch` (`0001-net-transport.patch`).
- Никаких изменений форматирования в чужих файлах: диффы должны читаться.
- Всё, что можно вынести в `coopnet` или в `gamedata/scripts/coop`, выносим туда.
  Патч движка — только там, где без него нельзя.
- Новые файлы движка лежат в `engine/patches/files/` и копируются при применении.

## Получение и сборка

```powershell
./engine/fetch.ps1                 # клон фиксированного коммита + сабмодули в engine/src
./engine/apply.ps1                 # копирование coopnet и наших патчей в engine/src
./engine/build.ps1                 # nuget restore + msbuild (Release x64 по умолчанию)
./engine/build.ps1 -Configuration Debug -Platform x86
```

`engine/src` в git не попадает: это чужие исходники, их место — на диске, а не в
истории мода. `fetch.ps1` проверяет, что HEAD совпадает с `engine/upstream.txt`,
и не продолжает при расхождении.

`apply.ps1` проверяет тот же коммит, копирует `engine/patches/files/` в дерево,
кладёт `coopnet/` в `src/xrNetServer/coopnet/` и применяет `engine/patches/*.patch`
в лексикографическом порядке. Уже применённый патч распознаётся и пропускается;
конфликт останавливает скрипт с выводом `git apply`. Автоматический `git am` без
проверки коммита запрещён — иначе получаем «почти собралось» и сутки отладки.

## Точки интеграции

Проверено по коду на коммите `d1eb5d3`:

- Windows-сборка (`engine.sln` → MSBuild) компилирует `src/xrNetServer/NET_Client.cpp`
  и `NET_Server.cpp`: там DirectPlay 8 (`IDirectPlay8Client`/`IDirectPlay8Server`).
  `src/xrNetServer/empty/` — заглушки того же интерфейса для CMake-сборки; интерфейсы
  классов общие, реализация транспорта различается.
- Клиент: `CLevel : IPureClient` (`src/xrGame/Level.cpp`); вход —
  `CLevel::Connect2Server(options)` (`Level_network.cpp`); исходящие пакеты —
  `IPureClient::Send(NET_Packet&)`, входящие — `IPureClient::OnMessage` →
  `CLevel::OnMessage` (`Level_network_messages.cpp`). `NET_Packet` — просто буфер,
  поэтому транспорт можно заменить, не трогая игровой слой.
- Сервер: `xrServer : IPureServer` (`src/xrGame/xrServer.cpp`), `OnMessage`,
  `SendTo`/`SendBroadcast` — сюда встаёт наш транспорт вместо DirectPlay.
- Консольные команды регистрируются в `src/xrEngine/xr_ioc_cmd.cpp` (`CCC_Register`,
  макросы `CMD1..CMD4`); кадровый тик — `CRenderDevice::FrameMove`
  (`src/xrEngine/device.cpp`), время — `dwTimeContinual` (мс).
- Спавн/деспавн сущностей и `net_id` — `src/xrGame/Level_network_spawn.cpp`,
  `xrServer_Objects*.cpp`; это следующий слой после транспорта.

## Патч 0001 — coopnet в сборке движка

- `engine/patches/files/xrNetServer/coop/` — мост `coop::Bridge*`, экспортируется из
  `xrNetServer.dll`: сессии на игровом потоке, без своего потока и таймеров.
  Через него уже ходят прикладные сообщения (`C_CHAT`): сервер пересылает строку
  всем, кроме автора, последняя строка доступна как `BridgeLastChat()`.
- `engine/patches/files/xrEngine/coop/coop_cmds.cpp` — команды `coop_listen [port]`,
  `coop_connect host[:port] [name]`, `coop_say <text>`, `coop_stop`, `coop_status`.
- `engine/patches/0001-coopnet-in-engine.patch` — `xrNetServer.vcxproj` компилирует
  `coopnet/src/*.cpp` (include `coopnet/include`), `xrEngine.vcxproj` — команды,
  `CCC_Register` их регистрирует, `FrameMove` тикает мост.
- `engine/patches/files/xrGame/coop/` — `coop::PublishLocalPlayer(CActor*)`: раз
  в кадр `CActor::UpdateCL` отдаёт мировой трансформ локального актора в
  `BridgePublishLocalPlayer` и имя уровня с игровыми часами в `BridgeSetWorldInfo`.
  Там же спавн: `Spawn_Write` локального актора отдаётся в `BridgeSetLocalSpawn`,
  полученные блобы (`BridgeTakeSpawns`) спавнятся через `Level().g_sv_Spawn` с
  локальным game ID из `0xF000+`, `LOCAL`/`ASPLAYER` снимаются, кукла остаётся
  включённой; `BridgeTakeDespawns` удаляет её.
- `engine/patches/0002-publish-local-player.patch` — вызов в `Actor.cpp` и
  `coop\coop_player.cpp` в `xrGame.vcxproj`.
- `engine/apply.ps1` копирует файлы и `coopnet/` в дерево и применяет патчи; повторный
  запуск пропускает уже применённое.

Проверено 2026-11-20: `apply.ps1` + `build.ps1` дают `xrEngine.exe`;
`dumpbin /exports xrNetServer.dll` показывает `coop::BridgeTick`, `BridgeStartServer`,
`BridgeStartClient`, `BridgeStop`, `BridgeStatus`, `BridgeActive`, `BridgePeerCount`,
`BridgeSay`, `BridgeLastChat`, `BridgePublishLocalPlayer`, `BridgeRemotePlayers`,
`BridgeSetWorldInfo`, `BridgeServerGameTime`, `BridgeSetLocalSpawn`,
`BridgeTakeSpawns`, `BridgeTakeDespawns`; `dumpbin /imports xrGame.dll`
подтверждает импорт `BridgePublishLocalPlayer`, `BridgeSetWorldInfo`,
`BridgeSetLocalSpawn`, `BridgeTakeSpawns` и `BridgeTakeDespawns`.
В игре не проверялось — нужна легальная копия Clear Sky 1.5.10.

## Сборка

Проверено 2026-11-20 на этой машине: `./engine/build.ps1` доводит решение до
`engine/src/bin/x64/Release/xrEngine.exe` (Release x64).

- Нужны: Visual Studio Build Tools 2022 (MSVC 14.44), MSBuild, Windows SDK 10.
  CMake для движка не используется — Windows-сборка идёт через `engine.sln`.
- Зависимости: сабмодули (`Externals/*`) и NuGet-пакеты SDL2 2.32.4; `build.ps1`
  сам скачивает `nuget.exe` в `tools/_local/` и делает restore.
- Выход: `engine/src/bin/x64/Release/` — `xrEngine.exe` и все DLL движка.
- Игра для сборки не нужна. Для запуска — легальная копия Clear Sky 1.5.10.