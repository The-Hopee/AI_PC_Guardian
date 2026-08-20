# ARCH-001 — Фундамент проекта

## Статус

Реализована. Windows/MSVC проверен локально. Конфигурация Linux/GCC реализована,
но локально не запускалась, потому что на машине разработки отсутствуют WSL и
Docker.

## Цель

Создать минимальный фундамент репозитория, который собирает Windows Agent и
Ubuntu Server из одного CMake-проекта, применяет согласованный стандарт C++ и
запускает автоматические smoke-тесты.

## Контекст

AI PC Guardian состоит из фонового Windows Agent, Ubuntu Server и будущего
Linux Analyzer. Продуктовую функциональность нельзя добавлять, пока структура
сборки не воспроизводится, а платформенные границы не определены.

## Обязательный набор инструментов

- C++17, расширения конкретного компилятора отключены;
- CMake 3.25 или новее;
- Windows: Visual Studio 2022, MSVC 19.30 или новее, x64;
- Linux: GCC 11 или новее;
- CTest для тестирования.

В рамках задачи не добавляются Qt, gRPC, Protobuf, Boost, OpenSSL и базы данных.

## Структура репозитория

```text
CMakeLists.txt
agent/
cmake/
common/
server/
tests/
```

Windows-сборка подключает `agent/`, Linux-сборка — `server/`.
Платформонезависимый код размещается в `common/`.

## Обязательные цели сборки

### `guardian-agent`

- собирается на Windows;
- создаёт `guardian-agent.exe`;
- выводит `AI PC Guardian Agent v0.1` и завершается с кодом `0`.

### `guardian-server`

- собирается на Linux;
- создаёт `guardian-server`;
- выводит `AI PC Guardian Server v0.1` и завершается с кодом `0`.

### Базовые тесты

- проверяют сгенерированную версию проекта;
- запускают платформенный executable и проверяют его вывод.

## Требования к CMake

- версия корневого проекта — `0.1.0`;
- заголовок версии генерируется из `cmake/version.hpp.in` в каталог сборки;
- сгенерированные файлы не добавляются в Git;
- `guardian_project_options` передаёт C++17 и флаги предупреждений;
- MSVC использует `/W4 /permissive- /utf-8`;
- GCC использует `-Wall -Wextra -Wpedantic`;
- параметр `BUILD_TESTING=OFF` отключает тестовые цели.

## Последовательность реализации

1. Создать корневой CMake-проект и проверки компилятора.
2. Добавить генерируемый заголовок версии.
3. Добавить платформенные исполняемые цели.
4. Добавить общий CMake-target.
5. Добавить smoke-тесты CTest.
6. Описать команды сборки на Windows и Ubuntu.

## Приёмочные проверки

### Windows

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
./build/agent/Debug/guardian-agent.exe
```

### Ubuntu

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++
cmake --build build
ctest --test-dir build --output-on-failure
./build/server/guardian-server
```

## Критерии готовности

- CMake успешно конфигурируется на Windows и Ubuntu.
- Платформенный executable собирается и выводит ожидаемую версию.
- Все тесты CTest проходят.
- Результаты сборки игнорируются Git.
- README содержит воспроизводимые команды сборки.

## Не входит в задачу

- сбор телеметрии;
- регистрация Windows Service;
- сеть, RPC и сериализация;
- пользовательский интерфейс;
- установочные пакеты;
- настройка CI/CD.

## Результаты проверки

- Windows: MSVC 19.44, сборка успешна;
- Windows CTest: успешно;
- Ubuntu/GCC: ожидает запуска в Linux-окружении.
