# AI PC Guardian

Кроссплатформенная основа лёгкого продукта для диагностики компьютера. Сейчас
проект содержит базовую систему сборки, типизированную C++-модель событий CPU/RAM,
Protobuf-контракт для их сериализации и gRPC-контракт приёма телеметрии.

## Статус разработки

| Задача | Статус | Результат |
| --- | --- | --- |
| `ARCH-001` | Реализована | CMake-фундамент, платформенные targets и CTest |
| `COMMON-001` | Реализована | Общая модель телеметрии CPU/RAM |
| `PROTO-001` | Реализована | Protobuf-схема, C++-конвертация и тесты сериализации |
| `SERVER-001` | Реализована | Запускаемый gRPC-сервер, обработчик и loopback-тест полного RPC-пути |
| `WIN-001` | Запланирована | Windows gRPC-клиент для отправки тестового события |
| `DEPLOY-001` | Запланирована | Воспроизводимый Docker-образ Linux-сервера |
| `SEC-001` | Запланирована | TLS и аутентификация устройств для удалённого gRPC |
| `CLOUD-001` | Запланирована | Развёртывание серверного контейнера в Timeweb Cloud |
| `AI-001` | Запланирована | OpenRouter с закрытым списком доверенных моделей |
| `BILLING-001` | Запланирована | AI-кредиты, измерение себестоимости и прибыльные тарифы |

Подробные ТЗ и критерии приёмки находятся в
[docs/tasks](docs/tasks/README.md).

Для AI-функций принят закрытый список семейств моделей: Claude Sonnet, Claude
Opus и GPT-5.6 Sol. API-ключ хранится только на сервере, а клиент выбирает
логическое имя из разрешённого списка, но не передаёт произвольный provider ID.
Монетизация AI планируется через внутренние кредиты с фиксированной ценой режима
для пользователя и расчётом фактической себестоимости каждого запроса на сервере.

## Инструменты

- CMake 3.25 или новее;
- C++17 без нестандартных расширений компилятора;
- Windows: Visual Studio 2022, MSVC 19.30 или новее;
- Ubuntu/Linux: GCC 11 или новее.

Этап `SERVER-001` использует gRPC `v1.83.0` и входящий в него Protocol Buffers
`v35.1`. Обе зависимости собираются статически из закреплённых версий.

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

## Как связаны этапы

```text
COMMON-001                  PROTO-001                    SERVER-001
TelemetryEvent  →  guardian.v1.TelemetryEvent  →  gRPC-приём телеметрии
доменная модель     сериализуемый контракт         сетевой транспорт
```

`Guardian::Common` описывает данные, с которыми удобно и безопасно работает
бизнес-логика C++. `Guardian::Proto` переводит их в формат, который можно
превратить в байты, записать в файл или передать по сети. `guardian-server`
слушает loopback-интерфейс и принимает такие события через gRPC.

Краткое введение в используемые конструкции находится в
[руководстве по Protobuf](docs/guides/protobuf-basics.md).

## Сборка на Windows

Команды выполняются в PowerShell при установленной Visual Studio 2022:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
./build/agent/Debug/guardian-agent.exe
./build/server/Debug/guardian-server.exe
```

Ожидаемый вывод:

```text
AI PC Guardian Agent v0.1
Server listening on 127.0.0.1:50051
```

Параметры запуска сервера:

```powershell
./build/server/Debug/guardian-server.exe --help
./build/server/Debug/guardian-server.exe --version
./build/server/Debug/guardian-server.exe --address 127.0.0.1:50052
```

По умолчанию сервер доступен только с этого компьютера. Он работает до `Ctrl+C`,
после чего штатно завершает gRPC и освобождает порт.

## Сборка на Ubuntu

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++
cmake --build build
ctest --test-dir build --output-on-failure
./build/server/guardian-server
```

Ожидаемый вывод после запуска сервера:

```text
Server listening on 127.0.0.1:50051
```

Сервер работает до остановки процесса (`Ctrl+C`). Для сборки без тестовых
targets используйте `-DBUILD_TESTING=OFF`. Реализация и все восемь тестов
проверены на Windows/MSVC; контрольная сборка на Ubuntu/GCC ещё не выполнена.

## Структура репозитория

```text
agent/       Точка входа фонового Windows Agent
cmake/       CMake-шаблоны и настройка версии
common/      Общие идентификаторы, метрики и модель событий
proto/       Protobuf-схемы, генерация C++ и преобразование модели
docs/guides/ Учебные материалы по технологиям проекта
docs/tasks/  Подробные ТЗ и статусы реализации
server/      Обработчик gRPC-сервиса и точка входа Ubuntu Server
tests/       Тесты фундамента, модели и сериализации
```
