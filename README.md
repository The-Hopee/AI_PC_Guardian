# AI PC Guardian

Кроссплатформенная основа лёгкого продукта для диагностики компьютера. Сейчас
проект содержит базовую систему сборки и типизированную C++-модель событий
телеметрии CPU и оперативной памяти.

## Статус разработки

| Задача | Статус | Результат |
| --- | --- | --- |
| `ARCH-001` | Реализована | CMake-фундамент, платформенные targets и CTest |
| `COMMON-001` | Реализована | Общая модель телеметрии CPU/RAM |
| `PROTO-001` | Запланирована | Первая protobuf-схема и слой преобразования C++ |

Подробные ТЗ и критерии приёмки находятся в
[docs/tasks](docs/tasks/README.md).

## Инструменты

- CMake 3.25 или новее;
- C++17 без нестандартных расширений компилятора;
- Windows: Visual Studio 2022, MSVC 19.30 или новее;
- Ubuntu/Linux: GCC 11 или новее.

Реализованные этапы пока не добавляют сторонних runtime-зависимостей.

## Текущая модель данных

Публичная модель доступна через target `Guardian::Common`:

```cpp
#include <guardian/model/telemetry_event.hpp>

using namespace guardian::model;

TelemetryEvent event{
    EventId{"event-1"},
    Timestamp{},
    DeviceId{"device-1"},
    std::nullopt,
    CpuMetric{42.0},
};

if (!is_valid(event)) {
    // Некорректную телеметрию нельзя сохранять или отправлять.
}
```

`EventPayload` — это `std::variant<CpuMetric, MemoryMetric>`. Активный payload
определяет тип события, поэтому модель не может содержать противоречащие друг
другу поля типа и payload.

## Сборка на Windows

Команды выполняются в PowerShell при установленной Visual Studio 2022:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
./build/agent/Debug/guardian-agent.exe
```

Ожидаемый вывод:

```text
AI PC Guardian Agent v0.1
```

## Сборка на Ubuntu

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++
cmake --build build
ctest --test-dir build --output-on-failure
./build/server/guardian-server
```

Ожидаемый вывод:

```text
AI PC Guardian Server v0.1
```

Для сборки без тестовых targets используйте `-DBUILD_TESTING=OFF`.

## Структура репозитория

```text
agent/       Точка входа фонового Windows Agent
cmake/       CMake-шаблоны и настройка версии
common/      Общие идентификаторы, метрики и модель событий
docs/tasks/  Подробные ТЗ и статусы реализации
server/      Точка входа Ubuntu Server
tests/       Тесты фундамента и общей модели
```
