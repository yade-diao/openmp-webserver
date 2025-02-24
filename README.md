# openmp-webserver

## 项目描述

本项目是一个“旧系统后台任务优化实验项目”，目标不是重写整套系统，而是验证在不进行大规模重构时，如何用低侵入方式提升后台 CPU 密集任务吞吐。

实验背景如下：

1. 目标旧系统未采用 C++11 标准并发抽象。
2. 后台任务存在明显 CPU 热点，吞吐不足。
3. 业务侧希望先做阶段性性能缓解，再逐步推进系统重构。

在该背景下，本项目重点验证：

1. OpenMP 是否能通过“编译开关 + 少量热点循环并行标注”快速提升性能。
2. 与传统后台并发实现相比，工作量与收益是否更优。
3. 该方案是否足以作为重构前的临时策略。

## 本项目对比的三种方案

1. 串行方案（Serial）
2. 旧系统常见方案（pthread + 条件变量任务队列）
3. OpenMP 并行优化方案

三者在同一数据与同一计算模型下对比耗时和加速比，用于评估改造价值。

## 结论表达（用于报告）

1. OpenMP 方案的代码改造成本最低，适合快速落地。
2. 旧系统 pthread 队列方案工程实现更复杂，维护成本更高。
3. 在当前任务模型下，OpenMP 可提供可观加速，但属于阶段性优化，不替代系统重构。
4. 长期建议仍是重构并引入更现代化并发与任务调度架构。

## 工程实现范围

1. 网络服务框架：非阻塞 socket + epoll（LT/ET）+ Reactor/Proactor。
2. 在线请求路径：保持轻量，不在主路径执行重 CPU 处理。
3. 后台任务路径：上传后异步处理，支持 `openmp` 与 `std` 两后端切换。
4. 对比程序：单独提供旧系统风格三项对比可执行文件。

## 构建

环境要求：Linux/WSL2、CMake 3.20+、C++20 编译器、SQLite3 开发包、OpenMP 工具链。

```bash
cmake -S . -B build-linux
cmake --build build-linux -j
```

## 主服务运行

```bash
./build-linux/openmp_webserver [port] [workerCount] [lt|et] [reactor|proactor] [staticRoot] [dbPath] [logPath] [sync|async] [processRounds] [computeThreads] [openmp|std]
```

关键参数：

1. `processRounds`：后台 CPU 处理轮数。
2. `computeThreads`：后台计算线程数。
3. `openmp|std`：后台计算后端。

## 旧系统风格三项对比程序

对比程序文件： [src/legacy_backend_compare.cpp](src/legacy_backend_compare.cpp)

运行方式：

```bash
./build-linux/legacy_backend_compare [dataMB] [rounds] [workers]
```

示例：

```bash
./build-linux/legacy_backend_compare 8 1024 4
```

输出字段说明：

1. `serial_us`：串行耗时。
2. `legacy_pthread_queue_us`：pthread 队列方案耗时。
3. `openmp_us`：OpenMP 方案耗时。
4. `speedup_pthread_vs_serial`：pthread 相对串行加速比。
5. `speedup_openmp_vs_serial`：OpenMP 相对串行加速比。
6. `speedup_openmp_vs_pthread`：OpenMP 相对 pthread 加速比。

## 关于“C++11 之前”与 OpenMP 语法

需要明确两点：

1. OpenMP 并行指令语法（如 `parallel for`、`reduction`、`schedule`、`num_threads`）本身早于 C++11，并不依赖 C++11 才存在。
2. 本仓库当前实现是现代 C++ 工程代码，用于实验验证；“旧系统风格”是通过对比程序模拟任务模型与改造路径，而不是完整回退到旧编译器语法栈。
