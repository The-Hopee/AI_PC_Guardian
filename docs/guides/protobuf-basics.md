# Protobuf в AI PC Guardian

## Главное в одном предложении

Protocol Buffers — это язык описания данных и генератор кода, который превращает
описанную структуру в C++-классы, умеющие сохраняться в компактные байты и
восстанавливаться из них.

Protobuf сам по себе не отправляет данные по сети. За сетевую передачу на
следующем этапе будет отвечать gRPC.

## Зачем проекту две похожие модели

В проекте существуют два представления одного события:

```text
guardian::model::TelemetryEvent
              ↕ to_proto()/from_proto()
guardian::v1::TelemetryEvent
              ↕ SerializeToString()/ParseFromString()
             bytes
```

- `guardian::model` — удобная доменная модель C++: `std::variant`,
  `std::optional`, `std::chrono`, валидация и методы предметной области;
- `guardian::v1` — сгенерированная wire-модель: стабильный контракт для файлов,
  сети и программ на других языках;
- функции преобразования не дают деталям Protobuf распространиться по всей
  бизнес-логике.

Если позже формат передачи изменится, доменная модель не обязана меняться вместе
с ним.

## Как читать `.proto`

Минимальный пример из проекта:

```proto
syntax = "proto3";

package guardian.v1;

message CpuMetric {
  double usage_percent = 1;
}
```

- `syntax = "proto3"` выбирает версию синтаксиса;
- `package guardian.v1` становится namespace `guardian::v1` в C++;
- `message` описывает передаваемую структуру;
- `double usage_percent = 1` задаёт тип, имя и номер поля;
- номер `1` — идентификатор поля в бинарном формате, а не порядковый номер для
  красоты. После публикации его нельзя отдавать другому полю.

## `optional` и `oneof`

```proto
optional uint32 process_id = 4;
```

`optional` различает два состояния: поля нет и поле присутствует со значением
`0`. В C++ это проверяется через `has_process_id()`.

```proto
oneof payload {
  CpuMetric cpu_metric = 10;
  MemoryMetric memory_metric = 11;
}
```

`oneof` хранит ровно один вариант payload и выполняет роль, похожую на
`std::variant`. В C++ активный вариант читается через `payload_case()`, а вызов
`mutable_cpu_metric()` или `mutable_memory_metric()` переключает активный вариант.

## Что генерирует `protoc`

CMake запускает компилятор схем `protoc` и создаёт в build-каталоге:

```text
build/proto/generated/guardian/v1/telemetry.pb.h
build/proto/generated/guardian/v1/telemetry.pb.cc
```

Эти файлы не редактируются вручную и не коммитятся. Они полностью выводятся из
`telemetry.proto`.

Типичный сгенерированный API:

| `.proto` | Чтение C++ | Запись C++ |
| --- | --- | --- |
| `string event_id` | `event_id()` | `set_event_id(...)` |
| `optional uint32 process_id` | `process_id()` | `set_process_id(...)` |
| optional presence | `has_process_id()` | `clear_process_id()` |
| `CpuMetric cpu_metric` | `cpu_metric()` | `mutable_cpu_metric()` |
| `oneof payload` | `payload_case()` | выбирается через `mutable_*()` |

## Что сделали функции преобразования

`to_proto()`:

1. проверяет доменное событие через `model::is_valid()`;
2. копирует метаданные в сгенерированное сообщение;
3. переводит `std::optional` в protobuf presence;
4. через `std::visit` переводит активный `std::variant` в `oneof`.

`from_proto()` делает обратное:

1. смотрит `payload_case()`;
2. создаёт соответствующий вариант доменного payload;
3. восстанавливает идентификаторы, время и process ID;
4. снова вызывает `model::is_valid()`, потому что данные могли прийти извне.

## Где появляется настоящая сериализация

```cpp
std::string bytes;
message.SerializeToString(&bytes);

guardian::v1::TelemetryEvent parsed;
parsed.ParseFromString(bytes);
```

После первой строки `bytes` — уже бинарное представление сообщения. Именно его
в будущем сможет передавать gRPC. Тест `guardian.telemetry-conversion` проверяет
полный путь модель → Protobuf → bytes → Protobuf → модель.

## Что важно запомнить о совместимости

- не менять номер существующего поля;
- не использовать удалённый номер для нового поля, лучше помечать его
  `reserved`;
- новые поля добавлять под новыми номерами;
- неизвестные поля старый клиент обычно может пропустить, поэтому схема может
  развиваться без одновременного обновления всех программ;
- Protobuf не проверяет наши бизнес-правила вроде CPU от 0 до 100 — для этого
  остаётся `model::is_valid()`.

## Учебный маршрут

1. Прочитать официальный [C++ tutorial](https://protobuf.dev/getting-started/cpptutorial/)
   до первого `SerializeToString()` и `ParseFromString()`.
2. Сопоставить его пример с `telemetry.proto` и сгенерированным
   `telemetry.pb.h` в build-каталоге.
3. Просмотреть разделы `message`, field numbers, `optional`, `oneof` и package в
   [proto3 language guide](https://protobuf.dev/programming-guides/proto3/).
4. Запустить `guardian-proto-tests` под отладчиком и пройти по одному CPU
   round-trip шаг за шагом.
5. Перед следующим этапом пройти официальный
   [gRPC C++ basics tutorial](https://grpc.io/docs/languages/cpp/basics/).

Видео для первого знакомства:

- [Quick Practical Introduction to Protobuf: C++ example](https://www.youtube.com/watch?v=R9yPMnCbpJY)
  — короткий практический путь `.proto → protoc → C++`;
- [Protocol Buffers Crash Course — Hussein Nasser](https://www.youtube.com/watch?v=46O73On0gyI)
  — хорошее объяснение идеи сериализации и сравнение с JSON.

Видео можно смотреть с автоматическими русскими субтитрами. Команды установки из
видео копировать не нужно: в Guardian зависимость и генерация уже настроены через
CMake.
