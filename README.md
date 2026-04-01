# openmp-webserver

## Project Background

Refactor-and-compare experiment for an old C++ backend system.
The project keeps the original workflow and compares three task-processing implementations: pre-C++11 serial, pre-C++11 pthread, and OpenMP.

## Project Overview

Minimal C++ web server focused on background tasks.
Core flow: upload -> async hash processing -> encrypted at rest -> checksum/meta output.
Download flow: read encrypted file -> decrypt in server -> return plaintext bytes.
Backend implementation is selected at startup via args: serial, pthread, or openmp.

## Performance Summary

Test parameters: data_mb=64, rounds=512, workers=16 (3 runs, average).

1. pre-C++11 serial: 44,073,822 us
2. pre-C++11 pthread: 8,418,781 us
3. OpenMP: 7,290,087 us

Speedup:

1. pthread vs serial: 5.24x
2. OpenMP vs serial: 6.05x
3. OpenMP vs pthread: 1.15x

## Optimization Difficulty

In practice, OpenMP is usually easier to adopt in legacy code because you can keep most computation code unchanged and add parallel statements incrementally. pthread-based optimization is more invasive and often requires redesigning execution flow and data ownership across the project.

## Directory Structure

```text
openmp-webserver/
    CMakeLists.txt
    README.md
    src/
        main.cpp
        server.h
        server.cpp
        processor.h
        processor.cpp
        backend_serial.h
        backend_serial.cpp
        backend_pthread.h
        backend_pthread.cpp
        backend_openmp.h
        backend_openmp.cpp
    static/
        uploads/
```
