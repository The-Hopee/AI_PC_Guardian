# AI PC Guardian

Кроссплатформенная основа лёгкого продукта для диагностики компьютера. Сейчас
проект содержит базовую систему сборки, типизированную C++-модель событий CPU/RAM,
Protobuf-контракт для их сериализации, gRPC-сервер и первый Windows-клиент,
периодически отправляющий настоящую CPU/RAM-телеметрию через WinAPI. Первый
Qt 6/QML dashboard получает последний снимок от Agent через защищённый локальный
Named Pipe и реализован как опциональный Windows target.

## Статус разработки

| Задача | Статус | Результат |
| --- | --- | --- |
| `ARCH-001` | Реализована | CMake-фундамент, платформенные targets и CTest |
| `COMMON-001` | Реализована | Общая модель телеметрии CPU/RAM |
| `PROTO-001` | Реализована | Protobuf-схема, C++-конвертация и тесты сериализации |
| `SERVER-001` | Реализована | Запускаемый gRPC-сервер, обработчик и loopback-тест полного RPC-пути |
| `WIN-001` | Реализована | Windows gRPC-клиент, CLI и отправка тестового события |
| `WIN-002` | Реализована | Сбор настоящих CPU/RAM через WinAPI и отправка двух событий |
| `WIN-003` | Реализована | Периодический Agent, интервалы, `--once` и штатная остановка |
| `GUI-001` | Проверена | Qt/QML dashboard с живыми CPU/RAM |
| `TOOLS-001` | Реализована | Потокобезопасные консольные и файловые логи |
| `IPC-001` | Реализована | Защищённый Named Pipe Agent ↔ Desktop, framing и reconnect |
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
Первичный коммерческий рынок — Россия: пользовательские цены, пополнения и отчёты
выражаются в российских рублях. Международные валюты и локализации относятся к
последующим версиям.

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
WIN-002             WIN-001/PROTO-001                   SERVER-001
WinAPI → модели → TelemetryClient → protobuf/gRPC → приём телеметрии
```

`Guardian::Common` описывает данные, с которыми удобно и безопасно работает
бизнес-логика C++. `Guardian::Proto` переводит их в формат, который можно
превратить в байты, записать в файл или передать по сети. `TelemetryClient`
создаёт gRPC-вызов с deadline, а `guardian-server` слушает loopback-интерфейс и
принимает событие.

Краткое введение в используемые конструкции находится в
[руководстве по Protobuf](docs/guides/protobuf-basics.md).

## Сборка на Windows

Команды выполняются в PowerShell при установленной Visual Studio 2022:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

В первом PowerShell запустите сервер:

```powershell
./build/server/Debug/guardian-server.exe
```

Во втором PowerShell запустите периодическую отправку CPU/RAM:

```powershell
./build/agent/Debug/guardian-agent.exe --server 127.0.0.1:50051
```

Вывод сервера и агента содержит время, уровень, поток, компонент и место вызова:

```text
2026-09-01 12:00:00.000 [INFO] [thread 1234] [server.lifecycle] gRPC server is listening; address=127.0.0.1:50051, bound_port=50051 (main.cpp:100)
```

Те же записи немедленно сохраняются в `%LOCALAPPDATA%/AI_PC_Guardian/logs`:

- `guardian-agent.log`;
- `guardian-server.log`.

Каждая строка принудительно сбрасывается на диск. Поэтому после аварии журнал
показывает последние успешно пройденные операции. Необработанные C++-исключения,
завершающие процесс через `std::terminate`, также фиксируются перед остановкой.

Параметры запуска сервера:

```powershell
./build/server/Debug/guardian-server.exe --help
./build/server/Debug/guardian-server.exe --version
./build/server/Debug/guardian-server.exe --address 127.0.0.1:50052
```

Параметры запуска агента:

```powershell
./build/agent/Debug/guardian-agent.exe --help
./build/agent/Debug/guardian-agent.exe --version
./build/agent/Debug/guardian-agent.exe --server 127.0.0.1:50051
./build/agent/Debug/guardian-agent.exe --interval-ms 2000
./build/agent/Debug/guardian-agent.exe --once
```

По умолчанию сервер доступен только с этого компьютера. Он работает до `Ctrl+C`,
после чего штатно завершает gRPC и освобождает порт.

## Первый Qt/QML-интерфейс

`guardian-desktop` показывает живые CPU/RAM, полученные от работающего
`guardian-agent` через локальный Named Pipe. GUI больше не вызывает Windows
collectors: Agent остаётся единственным владельцем WinAPI и параллельно отправляет
телеметрию на gRPC-сервер. При остановке Agent последнее значение остаётся на
экране с отметкой об устаревании, а Qt-клиент переподключается автоматически.

На Windows-машине разработки установлен Qt 6.8.3 для MSVC 2022. Если Qt не
найден, CMake по-прежнему пропускает `guardian-desktop`, не ломая остальные
targets.

Для повторной сборки настройте путь к установленному Qt:

```powershell
cmake -S . -B build-grpc `
  -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64"
cmake --build build-grpc --config Debug --target guardian-desktop
$env:Path = "C:\Qt\6.8.3\msvc2022_64\bin;$env:Path"
./build-grpc/Debug/guardian-desktop.exe
```

Сначала запустите `guardian-agent`, затем `guardian-desktop`. Удалённый
`guardian-server` для отображения последнего локального снимка не обязателен.
GUI отключается явно через `-DGUARDIAN_BUILD_DESKTOP=OFF`. Подробные ТЗ:
[GUI-001](docs/tasks/GUI-001.md) и [IPC-001](docs/tasks/IPC-001.md).

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
targets используйте `-DBUILD_TESTING=OFF`. Реализация и все восемнадцать тестов
проверены на Windows/MSVC; контрольная сборка на Ubuntu/GCC ещё не выполнена.

## Структура репозитория

```text
agent/       Windows Agent, gRPC-клиент и сборщики системных метрик
cmake/       CMake-шаблоны и настройка версии
common/      Общие идентификаторы, метрики и модель событий
desktop/     Опциональный Qt 6/QML интерфейс Windows
proto/       Protobuf-схемы, генерация C++ и преобразование модели
docs/guides/ Учебные материалы по технологиям проекта
docs/tasks/  Подробные ТЗ и статусы реализации
server/      Обработчик gRPC-сервиса и точка входа Ubuntu Server
tests/       Тесты фундамента, модели и сериализации
tools/       Общая инфраструктура логирования
```
