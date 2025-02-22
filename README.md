# openmp-webserver

## 项目定位

本项目用于模拟和验证旧系统后台任务优化方案。

目标系统背景：

1. 旧系统未采用 C++11 标准并发抽象。
2. 后台存在 CPU 密集任务，吞吐成为阶段性瓶颈。
3. 业务要求在不重构整体架构前提下先提升性能。

因此，本项目采用 OpenMP 进行低侵入并行化改造，核心目标是：

1. 通过编译开关启用并行能力。
2. 通过少量热点循环并行标注利用多核。
3. 通过线程数可配置方式完成性能对比与风险控制。

## 核心结论

1. 在 C++11 `std` 后台并发方案上，对该任务模型并未观察到显著提升。
2. 对“未使用 C++11 并发抽象”的旧系统形态，采用 OpenMP 做后台并行化，在较低改造成本下可获得一定收益。
3. 该方案可在系统重构引入新特性之前提供短期性能缓解。
4. OpenMP 改造属于阶段性措施，长期建议仍是系统重构与并发模型升级。

## 当前实现范围

1. 网络层：非阻塞 socket + epoll（LT/ET）+ Reactor/Proactor。
2. 在线路径：保持轻量，避免在请求主路径执行重 CPU 计算。
3. 后台路径：上传后异步执行 CPU 任务，支持两种后端对比：
   - `openmp`
   - `std`（`std::thread + condition_variable` 任务队列）
4. 存储与日志：SQLite 用户数据、同步/异步日志。

## 构建与运行

环境要求：Linux/WSL2、CMake 3.20+、C++20 编译器、SQLite3 开发包、OpenMP 工具链。

```bash
cmake -S . -B build-linux
cmake --build build-linux -j
```

```bash
./build-linux/openmp_webserver [port] [workerCount] [lt|et] [reactor|proactor] [staticRoot] [dbPath] [logPath] [sync|async] [processRounds] [computeThreads] [openmp|std]
```

参数说明：

1. `processRounds`：后台 CPU 处理轮数。
2. `computeThreads`：后台计算线程数。
3. `openmp|std`：后台后端选择。

## 对比建议

进行对比时建议固定同一负载，仅切换以下变量：

1. `computeThreads=1` 与 `computeThreads>1`。
2. `openmp` 与 `std` 后端。

输出指标建议同时观察：

1. 后台处理耗时（`process_us`）。
2. 端到端等待时间。
3. 失败率与尾延迟。

## 旧系统风格三项对比工具

项目包含独立可执行文件 [src/legacy_backend_compare.cpp](src/legacy_backend_compare.cpp)，用于模拟 C++11 之前常见后台任务实现风格，并对比三种方案：

1. 串行（未并行优化）
2. pthread + 条件变量任务队列（旧系统常见实现）
3. OpenMP 并行优化

运行方式：

```bash
./build-linux/legacy_backend_compare [dataMB] [rounds] [workers]
```

示例：

```bash
./build-linux/legacy_backend_compare 8 1024 4
```

输出字段：

1. `serial_us`
2. `legacy_pthread_queue_us`
3. `openmp_us`
4. `speedup_openmp_vs_serial`
5. `speedup_openmp_vs_pthread`

## 风险说明

1. OpenMP 对 CPU 密集、可并行循环收益更明显。
2. 对 I/O 主导或小粒度任务，收益可能有限。
3. 为避免过度并发，需独立调优 `workerCount` 与 `computeThreads`。
