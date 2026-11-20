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

Пока не автоматизировано, делается на M1:

1. Клонировать OpenXRay на зафиксированный коммит (см. `engine/README.md`).
2. Применить наши патчи из `engine/patches/` поверх.
3. Собрать по [инструкции OpenXRay](https://github.com/OpenXRay/xray-16/wiki/[EN]-How-to-build-and-setup-on-Windows).
4. Положить собранный `xrEngine.exe` в каталог установленной игры (не в репозиторий).

Требования к движку: x64, MSVC 2022, Windows SDK 10, см. вики апстрима. Точный список
зависимостей фиксируется в `engine/README.md` после первой успешной сборки — не по памяти.

## Запуск тестовой сессии (появится в M1)

```powershell
xrEngine.exe -coop_listen -coop_port 27015
xrEngine.exe -coop_connect 127.0.0.1:27015
```

Флаги — контракт из `docs/ARCHITECTURE.md`; до реализации считаются несуществующими.