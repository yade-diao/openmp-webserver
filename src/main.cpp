// 一个“最基本”的 Windows WebServer（Winsock 版本）。
//
// 你在面试里可以按这个顺序讲清楚整个服务器是怎么工作的：
// 1) WSAStartup()：初始化 Winsock（Windows 上必须）
// 2) socket()：创建监听 socket
// 3) bind()：把 0.0.0.0:port 绑定到监听 socket
// 4) listen()：进入被动监听状态
// 5) accept()：循环接收连接，得到 client socket
// 6) recv()/send()：读取请求字节，返回一个最小 HTTP 响应
// 7) shutdown()/closesocket()：关闭连接
// 8) WSACleanup()：清理 Winsock
//
// 这是最小可跑版本：单线程、一个连接处理完再处理下一个。
// 扩展到“高并发”的方向：线程池（限制线程数）/ IOCP（事件驱动）。

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

// 把 WSAGetLastError() 变成人类可读的信息（简单起见只打印数字码）。
void print_wsa_error(const char* what) {
    std::cerr << what << " failed, WSAGetLastError=" << WSAGetLastError() << "\n";
}

// RAII：构造时 WSAStartup，析构时 WSACleanup。
//
// 为什么需要它？
// - Windows 的 socket API 属于 Winsock 子系统。
// - 在调用 socket/bind/listen/accept/recv/send 之前，必须先 WSAStartup。
// - 忘记调用会导致 API 失败。
// - 用 RAII 可以避免“函数中途 return/throw 后忘了清理”。
class WinsockRuntime {
public:
    WinsockRuntime() {
        WSADATA wsa{};
        // 请求 Winsock 2.2 版本；一般 Windows 都支持。
        const int rc = ::WSAStartup(MAKEWORD(2, 2), &wsa);
        if (rc != 0) {
            // 注意：WSAStartup 失败时不能用 WSAGetLastError；rc 就是错误码。
            throw std::runtime_error("WSAStartup failed, rc=" + std::to_string(rc));
        }
    }

    ~WinsockRuntime() {
        // 对应清理：与 WSAStartup 配对。
        ::WSACleanup();
    }

    WinsockRuntime(const WinsockRuntime&) = delete;
    WinsockRuntime& operator=(const WinsockRuntime&) = delete;
};

// 创建、绑定、监听：返回一个处于 listen 状态的 socket。
SOCKET create_listen_socket(uint16_t port) {
    // 1) socket(): 创建一个 TCP socket
    // AF_INET   : IPv4
    // SOCK_STREAM: 面向连接的字节流（TCP）
    // IPPROTO_TCP: 明确指定 TCP
    SOCKET listenSock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET) {
        print_wsa_error("socket");
        throw std::runtime_error("socket() failed");
    }

    // 让端口更容易复用：服务重启时，避免 TIME_WAIT 导致 bind 失败的概率。
    // （Windows 上 SO_REUSEADDR 语义和 Linux 略不同，但作为基本设置是常见的。）
    BOOL reuse = TRUE;
    ::setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR,
                 reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    // 2) bind(): 绑定到 0.0.0.0:port
    // 0.0.0.0 表示“本机所有网卡/所有 IP”。
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // INADDR_ANY = 0.0.0.0
    addr.sin_port = htons(port);              // 网络字节序（大端）

    if (::bind(listenSock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        print_wsa_error("bind");
        ::closesocket(listenSock);
        throw std::runtime_error("bind() failed (port in use?)");
    }

    // 3) listen(): 进入被动监听。
    // SOMAXCONN：让系统选择一个合理的 backlog 上限。
    if (::listen(listenSock, SOMAXCONN) == SOCKET_ERROR) {
        print_wsa_error("listen");
        ::closesocket(listenSock);
        throw std::runtime_error("listen() failed");
    }

    return listenSock;
}

// 处理一个连接：读一点请求，然后回一个固定 HTTP 响应。
void handle_one_client(SOCKET clientSock) {
    // 4) recv(): 读取客户端发送的字节。
    // HTTP 请求本质就是文本（请求行 + headers + \r\n\r\n + body）。
    // 这里做最小实现：读一小段，不做完整解析。
    char buffer[4096];
    const int n = ::recv(clientSock, buffer, static_cast<int>(sizeof(buffer)), 0);
    if (n == SOCKET_ERROR) {
        print_wsa_error("recv");
        // 即使 recv 失败也要关连接。
    }

    // 5) send(): 返回一个最小 HTTP/1.1 响应。
    // 面试要点：一定要有 Content-Length，否则客户端可能不知道 body 多长。
    const char* body = "Hello from basic Winsock server\n";
    const int bodyLen = static_cast<int>(std::strlen(body));

    std::string resp;
    resp += "HTTP/1.1 200 OK\r\n";
    resp += "Content-Type: text/plain; charset=utf-8\r\n";
    resp += "Content-Length: " + std::to_string(bodyLen) + "\r\n";
    // 最简单：不做 keep-alive，明确告诉对端“我发完就关”。
    resp += "Connection: close\r\n";
    resp += "\r\n";
    resp += body;

    const char* p = resp.data();
    int remaining = static_cast<int>(resp.size());
    while (remaining > 0) {
        const int sent = ::send(clientSock, p, remaining, 0);
        if (sent == SOCKET_ERROR) {
            print_wsa_error("send");
            break;
        }
        p += sent;
        remaining -= sent;
    }

    // 6) shutdown()/closesocket(): 关闭连接。
    // shutdown 表示“我不再收/发数据了”，closesocket 释放 socket 资源。
    ::shutdown(clientSock, SD_BOTH);
    ::closesocket(clientSock);
}

} // namespace

int main(int argc, char** argv) {
    uint16_t port = 8080;
    if (argc >= 2) {
        port = static_cast<uint16_t>(std::atoi(argv[1]));
    }

    try {
        // 初始化 Winsock（对应析构时 WSACleanup）。
        WinsockRuntime winsock;

        // 创建监听 socket（socket + bind + listen）。
        SOCKET listenSock = create_listen_socket(port);

        std::cout << "Listening on http://127.0.0.1:" << port << "\n";

        // 7) accept(): 循环等待客户端连接。
        // accept 会“阻塞”等到有人连上；成功后返回一个新的 socket（clientSock）。
        // 注意：listenSock 只用于接连接；clientSock 才用于和某个客户端收发数据。
        for (;;) {
            sockaddr_in clientAddr{};
            int clientLen = sizeof(clientAddr);

            SOCKET clientSock = ::accept(listenSock, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
            if (clientSock == INVALID_SOCKET) {
                print_wsa_error("accept");
                continue; // 接受失败就继续下一次 accept
            }

            // 打印客户端地址（可选加分项）：能体现你知道连接来自哪里。
            char ip[INET_ADDRSTRLEN] = {0};
            ::inet_ntop(AF_INET, &clientAddr.sin_addr, ip, sizeof(ip));
            std::cout << "Accepted client " << ip << ":" << ntohs(clientAddr.sin_port) << "\n";

            // 单线程版本：处理完一个客户端再回到 accept。
            // 高并发扩展点：把 handle_one_client(clientSock) 放进线程池队列，
            //              或者改为 IOCP/异步 I/O。
            handle_one_client(clientSock);
        }

        // 正常情况下不会走到这里（上面是无限循环）。
        ::closesocket(listenSock);

    } catch (const std::exception& ex) {
        std::cerr << "Fatal: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
