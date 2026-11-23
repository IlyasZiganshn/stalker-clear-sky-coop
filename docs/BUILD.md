# Сборка

## Что нужно

| Компонент | Версия | Зачем |
|---|---|---|
| Visual Studio Build Tools 2022 | MSVC 14.4x | компилятор |
| CMake | ≥ 3.25 | сборка |
| Ninja | любой свежий | генератор (идёт с Build Tools) |
| Git | любой | версии |
| S.T.A.L.K.E.R.: Clear Sky | 1.5.10, Steam/GOG | запуск, не для сборки `coopnet` |

CMake и Ninja лежат внутри Build Tools:
`...\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin` и `...\Ninja`.

## `coopnet` (без движка и без игры)

Скрипт сам находит Build Tools через `vswhere`, загружает окружение MSVC и вызывает
CMake с пресетами:

```powershell
./scripts/build.ps1 -Test
```

Вручную из окружения с загруженным `vcvars64.bat`:

```powershell
cmake --preset default
cmake --build --preset default
ctest --preset default
```

Если CMake нет в `PATH`, он лежит в Build Tools:
`<VS>\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin`.
CMake и Ninja ставятся вместе с C++-нагрузкой Visual Studio.

Результат: `build/coopnet/coopnet.lib` и `build/coopnet/coopnet_tests.exe`.

## Движок (OpenXRay)

```powershell
./engine/fetch.ps1   # фиксированный коммит апстрима + сабмодули в engine/src
./engine/build.ps1   # nuget restore + msbuild, Release x64
```

Скрипт сборки сам находит Build Tools через `vswhere` и скачивает `nuget.exe` в
`tools/_local/` при первом запуске. Игра для сборки не нужна — только для запуска.

Наши патчи из `engine/patches/` применяются поверх зафиксированного коммита
(см. `engine/README.md`). Собранный `xrEngine.exe` кладётся в каталог установленной
игры (не в репозиторий).

Требования к движку: x64, MSVC 2022, Windows SDK 10. Проверено: сборка проходит,
результат — `engine/src/bin/x64/Release/xrEngine.exe` (см. `engine/README.md`).

## Запуск тестовой сессии

Собранный движок раскладывается по каталогу игры скриптом (только бинарники,
`gamedata` и сейвы не трогаются):

```powershell
./scripts/prepare_testcopy.ps1 -GameDir local\game-host
./scripts/prepare_testcopy.ps1 -GameDir local\game-client
./scripts/deploy.ps1 -GameDir local\game-host
./scripts/deploy.ps1 -GameDir local\game-client
```

Для второй копии нужен отдельный каталог: логи, сейвы и кэш шейдеров пишутся
рядом с игрой. `prepare_testcopy.ps1` подменяет `fsgame.ltx` на движковый, чтобы
`_appdata_` (логи, сейвы, кэш) оставалась внутри копии; без этого обе копии
писали бы в один каталог из инсталлятора. `-DryRun` показывает, что будет
скопировано.

В игре сессией управляют консольные команды (`~`):

```
coop_listen 27016            # хост: обычная одиночная игра слушает UDP
coop_connect 192.168.1.5:27016 Stalker   # имя необязательно
coop_say текст
coop_status                      # пиры, RTT, чужие позиции и биты движения
coop_dump [limit]                # ID, секция и позиция объектов уровня (по ID)
coop_stop
```

Обе стороны должны быть на одном уровне. Пошаговый чек-лист проверки —
[docs/TESTING.md](TESTING.md).

Сессию можно поднять и без GUI: движок запускается из каталога игры (иначе
`fsgame.ltx` и `$fs_root$` не найдутся), а свитч `-exec` выполняет консольные
команды при старте. `-exec` забирает остаток командной строки, поэтому идёт
последним:

```powershell
# хост
.\bin\x64\xrEngine.exe -cs -nointro -nogameintro -force_flushlog -exec "coop_listen 27016; start server(all/single/alife/new)"
# клиент
.\bin\x64\xrEngine.exe -cs -nointro -nogameintro -force_flushlog -exec "coop_connect 127.0.0.1:27016 Tester; start server(all/single/alife/new)"
```

`-force_flushlog` обязателен: без него лог буферизуется и до выхода процесса
пуст. `-nogameintro` пропускает вступительную катсцену, которую новая игра
запускает сразу после загрузки уровня: секвенсор замораживает обновление актора,
поэтому до его конца не происходит ничего сетевого (кукла не спавнится, ввод не
публикуется), а руками её приходится ждать. Сама загрузка уровня занимает ~25 с.
`start server(<спавн>/<тип игры>/<alife>/new)` берёт имя реестра спавнов
(`all.spawn`), а не локацию.

Две копии на одной машине проверяются скриптами: `./scripts/test_ingame_session.ps1`
(рукопожатие в меню) и `./scripts/test_coop_level.ps1` (сессия плюс загруженный
уровень и куклы игроков). Оба разбирают логи сторон и возвращают 0 только если
обе стороны увидели друг друга.