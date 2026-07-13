# async-runtime — Project Specification

## Проблема

Нет простого, понятного C++ runtime для async I/O который можно изучить, встроить в другой проект, и убедиться в его корректности и производительности воспроизводимыми бенчмарками.

## Цель

Написать две независимые вещи:

- **`libasync`** — header-only async I/O runtime на основе ручной реализации корутин поверх epoll
- **`bench`** — платформа для нагрузочного тестирования любого TCP сервера

---

## Часть 1: libasync

### Функциональные требования

- Регистрация fd с callback на read / write / error
- Запуск таймера с callback через N миллисекунд
- Запуск и остановка event loop
- Создание non-blocking TCP сервера одной функцией
- Подключение к TCP серверу одной функцией

### Нефункциональные требования

- **Header-only** — подключается одним `#include`
- **Zero external dependencies** — только Linux headers + libc
- **Single-threaded** — никаких мьютексов, никакого shared state
- Компилируется с `-Wall -Wextra -Werror`
- Покрыто тестами с ASan + UBSan

### Не входит в scope

- TLS
- UDP
- Thread pool
- Windows / macOS поддержка

---

## Часть 2: bench platform

### Функциональные требования

**Client:**

- Открыть N соединений к target (аргумент)
- Слать данные заданного размера с заданным интервалом
- Измерять latency каждого запроса (отправил → получил ответ)
- Измерять throughput (байт/сек)
- Работать заданное время T секунд

**Runner:**

- Принимать пресеты из JSON файла
- Запускать client с нужными параметрами
- Собирать результаты
- Писать итог в JSON: p50, p95, p99, p999 latency, throughput, errors

**Формат пресета:**

```json
{
  "name": "10k_connections",
  "target": "127.0.0.1:8080",
  "connections": 10000,
  "duration_sec": 30,
  "message_size_bytes": 64
}
```

### Нефункциональные требования

- bench не зависит от libasync — тестирует **любой** TCP сервер
- Результаты воспроизводимы — одинаковые пресеты дают одинаковые результаты
- CI запускает бенчмарки и падает если p99 > threshold

---

## Структура репозитория

```
async-runtime/
├── include/
│   └── async/
│       ├── event_loop.hpp
│       ├── coroutine.hpp
│       ├── socket.hpp
│       └── timer.hpp
├── examples/
│   └── echo_server.cpp
├── tests/
│   ├── test_event_loop.cpp
│   ├── test_coroutine.cpp
│   └── test_timer.cpp
├── bench/
│   ├── client.cpp
│   ├── runner.cpp
│   └── presets/
│       ├── baseline.json
│       └── 10k_connections.json
├── .github/
│   └── workflows/
│       ├── ci.yml
│       └── bench.yml
├── CMakeLists.txt
├── justfile
└── README.md
```

---

## Ветки и коммиты

### Ветки

| Ветка | Назначение |
|-------|-----------|
| `main` | Стабильная, только через PR |
| `dev` | Текущая разработка |
| `feature/xxx` | Фича ветки |
| `bench/xxx` | Изменения в bench platform |

**PR правило:** squash merge, линейная история.

### Conventional Commits

```
feat(event_loop): add timerfd support
fix(socket): handle EAGAIN on partial write
bench(runner): add p999 latency metric
test(coroutine): add resume after EAGAIN case
docs(readme): add architecture diagram
```

---

## CI/CD

### ci.yml — на каждый push

```
build (gcc + clang)
tests
sanitizers (asan + ubsan)
```

### bench.yml — на PR в main

```
запустить baseline.json пресет
сравнить с main веткой
оставить comment в PR с результатами
упасть если p99 деградировало > 10%
```

---

## Порядок разработки

| Шаг | Что делаем |
|-----|-----------|
| 1 | CMakeLists + justfile + CI skeleton |
| 2 | `socket.hpp` — non-blocking listen/connect |
| 3 | `event_loop.hpp` — epoll ET + dispatch |
| 4 | `coroutine.hpp` — state machine + resume() |
| 5 | `timer.hpp` — timerfd интеграция |
| 6 | `echo_server.cpp` — демо поверх libasync |
| 7 | `tests/` — unit тесты + sanitizers |
| 8 | `bench/client.cpp` |
| 9 | `bench/runner.cpp` |
| 10 | bench.yml в CI |
