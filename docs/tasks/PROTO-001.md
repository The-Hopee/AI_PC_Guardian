# PROTO-001 — Первое protobuf-сообщение

## Статус

Реализована. Protobuf `v35.1` подключён через дерево зависимостей gRPC,
C++-код генерируется во время сборки,
преобразования в обе стороны реализованы и покрыты тестами на Windows/MSVC.
Проверка на Ubuntu/GCC остаётся платформенной проверкой перед статусом
«Проверена».

## Цель

Добавить версионированный Protobuf-контракт для существующей телеметрии CPU/RAM
и протестированные C++-преобразования в обе стороны:

```text
guardian::model::TelemetryEvent
              ↕
guardian.v1.TelemetryEvent
```

Задача проверяет только границу сериализации. Передача событий по сети не
реализуется.

## Решение по зависимости

- версия Protocol Buffers — `v35.1`, закреплённая в git submodule выпуска gRPC
  `v1.83.0`;
- библиотека выполнения C++ собирается статически;
- проект сохраняет стандарт C++17;
- собственные тесты Protobuf отключаются в родительской сборке;
- сгенерированные C++-файлы создаются в build-каталоге и не коммитятся;
- CMake-интеграция публикует цель проекта и не меняет глобальные настройки.

Интеграция выполняется через CMake `FetchContent`. Начиная с `SERVER-001`,
владельцем дерева сторонних зависимостей является gRPC: проект использует его
штатный submodule Protobuf и не собирает вторую независимую копию библиотеки.
Запрещено использовать `main` или другую незакреплённую ветку.

## Предлагаемая структура

```text
proto/
├── CMakeLists.txt
├── guardian/
│   └── v1/
│       └── telemetry.proto
├── include/
│   └── guardian/
│       └── proto/
│           └── telemetry_conversion.hpp
└── src/
    └── telemetry_conversion.cpp

tests/
└── proto/
    └── telemetry_conversion_test.cpp
```

Сгенерированные `.pb.h` и `.pb.cc` размещаются только внутри `build/`.

## Схема передачи данных

Файл: `proto/guardian/v1/telemetry.proto`.

```proto
syntax = "proto3";

package guardian.v1;

message CpuMetric {
  double usage_percent = 1;
}

message MemoryMetric {
  uint64 total_bytes = 1;
  uint64 available_bytes = 2;
}

message TelemetryEvent {
  string event_id = 1;
  int64 timestamp_unix_ms = 2;
  string device_id = 3;
  optional uint32 process_id = 4;

  oneof payload {
    CpuMetric cpu_metric = 10;
    MemoryMetric memory_metric = 11;
  }
}
```

## Правила схемы

- имя package содержит версию API `v1` с первой схемы;
- номера опубликованных полей нельзя переиспользовать;
- номера payload начинаются с `10`, оставляя место метаданным оболочки;
- `oneof` является protobuf-эквивалентом C++ `std::variant`;
- отдельный сериализованный `EventType` не нужен: `oneof` уже определяет payload
  и не может ему противоречить;
- объёмы в байтах остаются unsigned 64-bit значениями;
- timestamp хранится как UTC Unix time в миллисекундах;
- `process_id` отличает отсутствие значения от числового нуля.

## Цель CMake

### `Guardian::Proto`

Цель должна:

- генерировать C++-исходники из `telemetry.proto` во время сборки;
- линковать библиотеку Protobuf в области видимости, необходимой сгенерированным
  заголовкам;
- публиковать build-каталог со сгенерированными заголовками;
- линковать `Guardian::Common` для слоя преобразования;
- передавать требование C++17 через свойства цели.

Глобальные `include_directories()` и `link_libraries()` запрещены.

## API преобразования

Заголовок: `guardian/proto/telemetry_conversion.hpp`.

```cpp
namespace guardian::proto {

std::optional<guardian::v1::TelemetryEvent> to_proto(
    const model::TelemetryEvent& event);

std::optional<model::TelemetryEvent> from_proto(
    const guardian::v1::TelemetryEvent& message);

}  // namespace guardian::proto
```

### Поведение `to_proto()`

- возвращает `std::nullopt` для невалидного доменного события;
- без изменений копирует event ID и device ID;
- преобразует timestamp в signed Unix milliseconds;
- задаёт `process_id` только при наличии optional-значения;
- через `std::visit` записывает соответствующий вариант `oneof`.

### Поведение `from_proto()`

- отклоняет отсутствующий или неизвестный payload;
- преобразует активный `oneof` в соответствующую альтернативу C++ variant;
- сохраняет состояние optional process ID;
- создаёт доменное событие и вызывает `model::is_valid()`;
- возвращает `std::nullopt` для некорректного сообщения.

Функции преобразования не помечаются `noexcept`: операции сгенерированного
Protobuf-кода и
выделение памяти для строк могут выбрасывать исключения.

## Обязательные тесты

1. Полное обратное преобразование корректного CPU-события: модель → Protobuf →
   модель.
2. Полное обратное преобразование корректного события памяти.
3. Event ID, device ID и timestamp сохраняются после обратного преобразования.
4. Отсутствующий process ID остаётся отсутствующим.
5. Заданный process ID сохраняет числовое значение.
6. CPU payload выбирает `kCpuMetric` в сгенерированном `oneof case`.
7. Memory payload выбирает `kMemoryMetric`.
8. Сообщение без payload отклоняется `from_proto()`.
9. Пустые event ID и device ID отклоняются.
10. Некорректные CPU и memory значения отклоняются в обоих направлениях.
11. Сериализованные байты читаются в новый экземпляр сгенерированного сообщения.

Тесты не должны зависеть от сети, gRPC, текущего времени и платформенных API.

## Последовательность реализации

1. Подключить закреплённую версию Protobuf.
2. Добавить `proto/guardian/v1/telemetry.proto`.
3. Настроить генерацию C++ в build-каталог.
4. Создать `Guardian::Proto`.
5. Реализовать преобразование доменная модель → Protobuf.
6. Реализовать преобразование Protobuf → доменная модель.
7. Добавить тесты обратного преобразования и некорректных данных.
8. Обновить статус, зависимости и команды в README.
9. Проверить чистую конфигурацию, сборку и CTest на Windows и Ubuntu.

## Критерии готовности

- версия Protobuf закреплена и выводится при CMake-конфигурации;
- сгенерированные C++-файлы не добавлены в Git;
- `Guardian::Proto` собирается MSVC и GCC;
- package и номера полей соответствуют этому ТЗ;
- все тесты преобразования и сериализации проходят;
- существующие тесты фундамента и модели остаются зелёными;
- README и статус задачи обновлены.

## Не входит в задачу

- gRPC-сервисы, клиенты и сеть;
- аутентификация, TLS и сжатие;
- пакетная отправка нескольких событий;
- реестр схем и хранение в базе данных;
- сообщения GPU, процессов, сети, игр и ошибок;
- инструменты миграции до публикации первой версии схемы.

## Официальные источники

- [Protocol Buffers v35.1](https://github.com/protocolbuffers/protobuf/releases/tag/v35.1)
- [Сборка Protobuf через CMake](https://github.com/protocolbuffers/protobuf/blob/main/cmake/README.md)
- [Документация сгенерированного C++](https://protobuf.dev/reference/cpp/cpp-generated/)
