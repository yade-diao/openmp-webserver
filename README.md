# openmp-webserver

## Project Background

This project is a refactor-and-compare experiment for an old C++ backend system.
It keeps the original workflow style and evaluates backend task processing performance across three implementations: pre-C++11 serial baseline, pre-C++11 pthread-based optimization, and OpenMP optimization.

## Project Overview

openmp-webserver is a minimal C++ web server focused on background task processing.
It supports file upload, asynchronous processing, checksum generation, and backend comparison across Serial, pthread, and OpenMP modes.

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
