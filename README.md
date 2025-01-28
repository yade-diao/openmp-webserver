# openmp-webserver（多线程高并发版）

这是一个 Windows/Winsock 的多线程服务器实现：
- `WSAStartup` 初始化
- `socket/bind/listen` 监听
- 主线程循环 `accept` 接收连接
- 固定大小线程池并行处理客户端请求
- 任务队列限流（队列满时快速返回 `503 Service Unavailable`）
- `recv` 读请求，`send` 回最小 HTTP 响应
- `shutdown/closesocket` 关闭连接
- `WSACleanup` 清理

## 构建（PowerShell）

```powershell
cmake -S . -B build
cmake --build build --config Release
```

## 运行

```powershell
./build/Release/openmp_webserver.exe 8080 16
```

参数说明：
- 第 1 个参数：端口（默认 `8080`）
- 第 2 个参数：worker 线程数（默认 `hardware_concurrency()`，获取失败时回退到 `8`）

然后浏览器访问：
- http://127.0.0.1:8080/

## 后续可继续优化

- IOCP：Windows 下的事件驱动异步 I/O，适合更高连接规模
- Keep-Alive：减少短连接频繁建连/断连开销
- 超时与连接管理：防慢连接长期占用资源
