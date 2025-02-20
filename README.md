# openmp-webserver

High-concurrency HTTP server for Linux/WSL, built with epoll, thread pool scheduling, SQLite-based identity, and asynchronous background processing.

## Architecture

- Network layer: non-blocking socket + epoll (LT/ET) + Reactor/Proactor event model.
- Request path: online path stays lightweight and does not run heavy CPU processing.
- Background compute: file upload triggers asynchronous processing with selectable backend:
	- OpenMP backend
	- std::thread + condition-variable task queue backend
- Persistence: SQLite for user registration/login.
- Logging: synchronous or asynchronous logger.

This design keeps tail latency stable on online requests while preserving CPU acceleration capability in offline/background tasks.

## Build Requirements

- Linux or WSL2
- CMake 3.20+
- C++20 compiler
- pthread
- SQLite3 dev package
- OpenMP runtime/toolchain

Example setup (Ubuntu):

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libsqlite3-dev
```

## Build

```bash
cmake -S . -B build-linux
cmake --build build-linux -j
```

Binary path:

```text
build-linux/openmp_webserver
```

## Runtime

```bash
./build-linux/openmp_webserver [port] [workerCount] [lt|et] [reactor|proactor] [staticRoot] [dbPath] [logPath] [sync|async] [processRounds] [computeThreads] [openmp|std]
```

Parameter definition:

1. `port` default `8080`
2. `workerCount` default `hardware_concurrency` (fallback `8`)
3. `lt|et` epoll trigger mode
4. `reactor|proactor` event model
5. `staticRoot` static file root
6. `dbPath` SQLite file path
7. `logPath` log file path
8. `sync|async` logger mode
9. `processRounds` CPU rounds used by background processing
10. `computeThreads` worker thread count for selected compute backend
11. `openmp|std` background compute backend

Speedup baseline guidance:

- single-core baseline: set `computeThreads=1`
- multi-core run: set `computeThreads` to physical core count or tuned value
- backend A/B: keep workload identical, then compare `openmp` vs `std`

## API Surface

- `POST /register`
- `POST /login`
- `GET /media/<filename>`
- `POST /upload?name=<filename>`
- `GET /files/<filename>`

Upload behavior under current architecture:

1. Upload returns immediately after persistence.
2. Background worker starts selected backend processing (`openmp` or `std`).
3. Task status is written to `/files/<filename>.task`.
4. Processing result metadata is written to `/files/<filename>.meta`.

## Operational Notes

- Run in Linux/WSL; epoll headers are not available in native Windows toolchains.
- Keep `workerCount` and `computeThreads` tuned independently to avoid CPU over-subscription.
- For performance analysis, keep workload and dataset fixed, and compare:
	- `computeThreads=1` vs `computeThreads>1`
	- backend `openmp` vs backend `std`
