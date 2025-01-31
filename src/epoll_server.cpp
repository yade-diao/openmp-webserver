#include "epoll_server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
constexpr int kMaxEvents = 1024;
constexpr int kReadBufferSize = 4096;

std::string buildSimpleResponse() {
    static const char* body = "Hello from epoll server\n";
    const int bodyLen = static_cast<int>(std::strlen(body));

    std::string resp;
    resp += "HTTP/1.1 200 OK\r\n";
    resp += "Content-Type: text/plain; charset=utf-8\r\n";
    resp += "Content-Length: " + std::to_string(bodyLen) + "\r\n";
    resp += "Connection: close\r\n\r\n";
    resp += body;
    return resp;
}

void sendAllAndClose(int fd, const std::string& response) {
    const char* p = response.data();
    int remaining = static_cast<int>(response.size());
    while (remaining > 0) {
        const int n = ::send(fd, p, remaining, 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        p += n;
        remaining -= n;
    }
    ::shutdown(fd, SHUT_RDWR);
    ::close(fd);
}
} // namespace

EpollServer::EpollServer(uint16_t port, std::size_t workerCount, TriggerMode triggerMode, EventModel eventModel)
    : port_(port),
      triggerMode_(triggerMode),
      eventModel_(eventModel),
      threadPool_(workerCount) {}

EpollServer::~EpollServer() {
    if (listenFd_ >= 0) {
        ::close(listenFd_);
    }
    if (epollFd_ >= 0) {
        ::close(epollFd_);
    }
}

void EpollServer::run() {
    initListenSocket();
    initEpoll();
    std::cout << "Listening on 0.0.0.0:" << port_
              << ", trigger=" << (triggerMode_ == TriggerMode::ET ? "ET" : "LT")
              << ", model=" << (eventModel_ == EventModel::Reactor ? "Reactor" : "Proactor")
              << "\n";
    eventLoop();
}

void EpollServer::initListenSocket() {
    listenFd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0) {
        throw std::runtime_error("socket() failed");
    }

    const int reuse = 1;
    ::setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    if (!setNonBlocking(listenFd_)) {
        throw std::runtime_error("setNonBlocking(listenFd) failed");
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port_);

    if (::bind(listenFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        throw std::runtime_error("bind() failed");
    }

    if (::listen(listenFd_, SOMAXCONN) < 0) {
        throw std::runtime_error("listen() failed");
    }
}

void EpollServer::initEpoll() {
    epollFd_ = ::epoll_create1(0);
    if (epollFd_ < 0) {
        throw std::runtime_error("epoll_create1() failed");
    }

    epoll_event ev{};
    ev.data.fd = listenFd_;
    ev.events = EPOLLIN;
    if (triggerMode_ == TriggerMode::ET) {
        ev.events |= EPOLLET;
    }

    if (::epoll_ctl(epollFd_, EPOLL_CTL_ADD, listenFd_, &ev) < 0) {
        throw std::runtime_error("epoll_ctl ADD listenFd failed");
    }
}

void EpollServer::eventLoop() {
    std::vector<epoll_event> events(kMaxEvents);

    for (;;) {
        const int ready = ::epoll_wait(epollFd_, events.data(), static_cast<int>(events.size()), -1);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw std::runtime_error("epoll_wait() failed");
        }

        for (int i = 0; i < ready; ++i) {
            const int fd = events[i].data.fd;
            const uint32_t ev = events[i].events;

            if (fd == listenFd_) {
                acceptClients();
                continue;
            }

            if (ev & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                closeClient(fd);
                continue;
            }

            if (ev & EPOLLIN) {
                handleClientReadable(fd);
            }
        }
    }
}

void EpollServer::acceptClients() {
    for (;;) {
        sockaddr_in clientAddr{};
        socklen_t len = sizeof(clientAddr);
        const int clientFd = ::accept(listenFd_, reinterpret_cast<sockaddr*>(&clientAddr), &len);
        if (clientFd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            }
            if (errno == EINTR) {
                continue;
            }
            std::cerr << "accept failed: errno=" << errno << "\n";
            return;
        }

        if (!setNonBlocking(clientFd)) {
            ::close(clientFd);
            continue;
        }

        addClient(clientFd);

        if (triggerMode_ == TriggerMode::LT) {
            return;
        }
    }
}

void EpollServer::handleClientReadable(int fd) {
    if (eventModel_ == EventModel::Reactor) {
        threadPool_.enqueue([this, fd]() { handleClientReactor(fd); });
    } else {
        handleClientProactor(fd);
    }
}

void EpollServer::handleClientReactor(int fd) {
    char buf[kReadBufferSize];
    std::string req;

    for (;;) {
        const int n = ::recv(fd, buf, sizeof(buf), 0);
        if (n > 0) {
            req.append(buf, buf + n);
            if (triggerMode_ == TriggerMode::LT) {
                break;
            }
            continue;
        }

        if (n == 0) {
            closeClient(fd);
            return;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break;
        }
        if (errno == EINTR) {
            continue;
        }

        closeClient(fd);
        return;
    }

    if (!req.empty()) {
        sendAllAndClose(fd, buildSimpleResponse());
    }
}

void EpollServer::handleClientProactor(int fd) {
    char buf[kReadBufferSize];
    std::string req;

    for (;;) {
        const int n = ::recv(fd, buf, sizeof(buf), 0);
        if (n > 0) {
            req.append(buf, buf + n);
            if (triggerMode_ == TriggerMode::LT) {
                break;
            }
            continue;
        }

        if (n == 0) {
            closeClient(fd);
            return;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break;
        }
        if (errno == EINTR) {
            continue;
        }

        closeClient(fd);
        return;
    }

    if (req.empty()) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(connMutex_);
        readBuffers_[fd].append(req);
    }

    // 模拟 Proactor：主线程先读完数据，再把“业务处理+回写”投递给线程池。
    threadPool_.enqueue([this, fd]() {
        std::string data;
        {
            std::lock_guard<std::mutex> lock(connMutex_);
            auto it = readBuffers_.find(fd);
            if (it != readBuffers_.end()) {
                data.swap(it->second);
                readBuffers_.erase(it);
            }
        }

        if (data.empty()) {
            return;
        }

        sendAllAndClose(fd, buildSimpleResponse());
    });
}

void EpollServer::addClient(int fd) {
    epoll_event ev{};
    ev.data.fd = fd;
    ev.events = EPOLLIN | EPOLLRDHUP;
    if (triggerMode_ == TriggerMode::ET) {
        ev.events |= EPOLLET;
    }

    if (::epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev) < 0) {
        ::close(fd);
    }
}

void EpollServer::closeClient(int fd) {
    ::epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, nullptr);
    {
        std::lock_guard<std::mutex> lock(connMutex_);
        readBuffers_.erase(fd);
    }
    ::shutdown(fd, SHUT_RDWR);
    ::close(fd);
}

bool EpollServer::setNonBlocking(int fd) {
    const int oldFlags = ::fcntl(fd, F_GETFL, 0);
    if (oldFlags < 0) {
        return false;
    }
    if (::fcntl(fd, F_SETFL, oldFlags | O_NONBLOCK) < 0) {
        return false;
    }
    return true;
}
