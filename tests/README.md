# tests

使用 Webbench 进行并发访问、并发上传、并发下载测试。

## 1. 安装 Webbench

如果系统没有 webbench，可先安装编译工具再手动编译 webbench。

```bash
sudo apt-get update
sudo apt-get install -y build-essential git
```

确认可用：

```bash
webbench -h
```

## 2. 启动服务器（示例）

```bash
./build-linux/openmp_webserver 8080 8 et reactor ./static ./webserver.db ./server.log async noomp 128
```

最后两个参数：

1. `omp|noomp`：上传/下载数据处理阶段是否开启 OpenMP
2. `processRounds`：每次上传/下载处理轮数（越大 CPU 压力越高，越容易看出差异）

## 3. 并发访问测试（Webbench）

```bash
bash tests/webbench_access.sh http://127.0.0.1:8080 200 30
```

参数：

1. base_url（默认 `http://127.0.0.1:8080`）
2. clients（默认 `200`）
3. seconds（默认 `30`）

## 4. 并发上传/下载测试（Webbench）

```bash
bash tests/webbench_upload_download.sh http://127.0.0.1:8080 100 200 20
```

参数：

1. base_url（默认 `http://127.0.0.1:8080`）
2. upload_clients（默认 `100`）
3. download_clients（默认 `200`）
4. seconds（默认 `20`）

## 5. 说明

1. 下载接口为公开接口：`GET /files/<filename>`。
2. 上传压测使用 `GET /upload?name=...&content=...`，用于兼容 Webbench 仅 URL 压测场景。
3. 业务上传仍支持 `POST /upload?name=...` + body。

## 6. OpenMP 开关对比（一键）

```bash
bash tests/compare_openmp.sh 18080 60 100 15 128
```

该脚本会自动运行两组：

1. `noomp`（关闭 OpenMP）
2. `omp`（开启 OpenMP）

并输出 upload/download 的 `Speed=... pages/min`，用于直接比较吞吐差异。
