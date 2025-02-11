# openmp-webserver

一个面向学习的 Linux/WSL C++ Web 服务器项目，已拆分为多个功能模块：

- 并发模型：线程池 + 非阻塞 socket + epoll
- 触发模式：支持 LT / ET
- 事件模型：支持 Reactor / 模拟 Proactor
- HTTP：状态机解析，支持 GET / POST
- 业务：用户注册 / 登录（SQLite）+ 图片/视频文件访问
- 日志：同步/异步日志

> 注意：当前实现依赖 epoll，仅支持 Linux/WSL 运行。

## 1. 项目结构

```text
openmp-webserver/
	CMakeLists.txt
	README.md
	src/
		main.cpp              # 程序入口：只负责参数解析与启动
		epoll_server.h/.cpp   # 网络核心：epoll、连接管理、事件分发
		thread_pool.h/.cpp    # 线程池
		http_parser.h/.cpp    # HTTP 状态机解析
		web_app.h/.cpp        # 业务路由层
		user_repository.h/.cpp# SQLite 用户注册/登录
		logger.h/.cpp         # 同步/异步日志
```

## 2. 环境准备（WSL Ubuntu）

在 WSL 中安装依赖：

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libsqlite3-dev
```

检查版本：

```bash
cmake --version
g++ --version
```

## 3. 编译

在项目根目录执行：

```bash
cmake -S . -B build-linux
cmake --build build-linux -j
```

生成的可执行文件一般在：

```text
build-linux/openmp_webserver
```

## 4. 运行

### 4.1 最简运行

```bash
./build-linux/openmp_webserver
```

### 4.2 完整参数运行

```bash
./build-linux/openmp_webserver 8080 8 et reactor ./static ./webserver.db ./server.log async
```

参数顺序：

1. `port`：监听端口，默认 `8080`
2. `workerCount`：线程池数量，默认 `hardware_concurrency()`（失败回退 8）
3. `lt|et`：epoll 触发模式
4. `reactor|proactor`：事件处理模型
5. `staticRoot`：静态资源根目录（用于 `/media/...`）
6. `dbPath`：SQLite 数据库文件路径
7. `logPath`：日志文件路径
8. `sync|async`：日志模式

## 5. HTTP 接口说明

### 5.1 注册

- 路径：`POST /register`
- Body（form 风格）：`username=alice&password=123456`

示例：

```bash
curl -i -X POST "http://127.0.0.1:8080/register" \
	-H "Content-Type: application/x-www-form-urlencoded" \
	-d "username=alice&password=123456"
```

### 5.2 登录

- 路径：`POST /login`
- Body（form 风格）：`username=alice&password=123456`

示例：

```bash
curl -i -X POST "http://127.0.0.1:8080/login" \
	-H "Content-Type: application/x-www-form-urlencoded" \
	-d "username=alice&password=123456"
```

### 5.3 访问图片/视频

- 路径：`GET /media/<filename>`
- 文件来源：`staticRoot` 指定目录

示例：

```bash
curl -i "http://127.0.0.1:8080/media/demo.jpg"
curl -i "http://127.0.0.1:8080/media/demo.mp4"
```

## 6. 核心流程（新手理解版）

1. `main.cpp` 解析参数，创建 `EpollServer`。
2. `EpollServer` 创建监听 socket，并设置为非阻塞。
3. 将监听 fd 注册到 epoll，进入 `epoll_wait` 循环。
4. 新连接到来时 `accept`，并把客户端 fd 加入 epoll。
5. 客户端可读时：
	 - Reactor：把读+处理任务投递到线程池。
	 - 模拟 Proactor：主线程先读，再把处理任务投线程池。
6. 读到的字节交给 `HttpParser` 做状态机解析：
	 - `Incomplete`：等待后续数据
	 - `Error`：返回 400
	 - `Complete`：交给业务层
7. `WebApp` 根据路径选择注册、登录或媒体文件访问。
8. `UserRepository` 通过 SQLite 完成用户数据读写。
9. 返回 HTTP 响应，并记录日志。

## 7. 日志

- 同步模式：业务线程直接写日志，简单但可能阻塞。
- 异步模式：日志先入队，后台线程写入，吞吐更好。

日志中会记录：

- 启动信息
- 连接接入
- 请求处理结果
- 解析/系统错误

## 8. 常见问题

### Q1: VS Code 报找不到 `sys/epoll.h`

这是因为你当前 IntelliSense 可能使用的是 Windows 头文件路径。项目应在 WSL/Linux 下编译运行。

### Q2: 运行时报数据库错误

确认安装了 `libsqlite3-dev`，并检查 `dbPath` 目录是否可写。

### Q3: 访问 `/media/...` 返回 404

确认文件实际存在于 `staticRoot` 目录下，且请求文件名拼写正确。

## 9. 当前实现的边界

这是一个教学导向实现，仍有可增强点：

- 密码目前为明文存储（建议改为哈希）
- HTTP 功能较基础（未实现 Keep-Alive、Chunked 等）
- 缺少完整自动化测试与压测脚本

但它已经具备清晰分层和可扩展骨架，适合继续迭代。
