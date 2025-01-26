# openmp-webserver（面试版最小服务器）

这是一个 Windows/Winsock 的“最基本服务器”实现：
- `WSAStartup` 初始化
- `socket/bind/listen` 监听
- 循环 `accept` 接收连接
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
./build/Release/openmp_webserver.exe 8080
```

然后浏览器访问：
- http://127.0.0.1:8080/

## 高并发扩展思路（下一步）

- 线程池：`accept` 后把 `clientSock` 作为任务提交到固定 worker 线程（限制线程数量）
- IOCP：Windows 下的事件驱动异步 I/O，适合几万连接
- 超时/限流：防慢连接占满资源
