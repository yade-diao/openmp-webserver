# openmp-webserver

## Project Background

Refactor-and-compare experiment for an old C++ backend system.
The project keeps the original workflow and compares three task-processing implementations: pre-C++11 serial, pre-C++11 pthread, and OpenMP.

## Project Overview

Minimal C++ web server focused on background tasks.
Core flow: upload -> async processing -> checksum/meta output -> backend comparison.

## Performance Summary

Test parameters: data_mb=64, rounds=512, workers=16 (3 runs, average).

1. pre-C++11 serial: 13,155,523 us
2. pre-C++11 pthread: 2,306,194 us
3. OpenMP: 2,486,154 us

Speedup:

1. pthread vs serial: 5.70x
2. OpenMP vs serial: 5.29x
3. OpenMP vs pthread: 0.93x

## Optimization Difficulty

1. pre-C++11 serial: Low
2. pre-C++11 pthread: High
3. OpenMP: Medium

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
        backend_compare.cpp
    static/
        uploads/
```
