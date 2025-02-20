#pragma once

#include "http_parser.h"
#include "logger.h"
#include "thread_pool.h"
#include "web_app.h"

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

enum class TriggerMode {
    LT,
    ET,
};

enum class EventModel {
    Reactor,
    Proactor,
};

class EpollServer {
public:
    EpollServer(uint16_t port,
                std::size_t workerCount,
                TriggerMode triggerMode,
                EventModel eventModel,
                const std::string& dbPath,
                const std::string& staticRoot,
                const std::string& logPath,
                LogMode logMode,
                int processRounds,
                std::size_t workerThreads,
                ComputeBackend backend);
    ~EpollServer();

    EpollServer(const EpollServer&) = delete;
    EpollServer& operator=(const EpollServer&) = delete;

    void run();

private:
    void initListenSocket();
    void initEpoll();
    void eventLoop();

    void acceptClients();
    void handleClientReadable(int fd);
    void handleClientReactor(int fd);
    void handleClientProactor(int fd);
    void processAndReply(int fd, const std::string& data);

    void addClient(int fd);
    void closeClient(int fd);
    static bool setNonBlocking(int fd);

    int listenFd_{-1};
    int epollFd_{-1};
    uint16_t port_;
    TriggerMode triggerMode_;
    EventModel eventModel_;
    ThreadPool threadPool_;
    HttpParser parser_;
    WebApp app_;
    Logger logger_;

    std::mutex connMutex_;
    std::unordered_map<int, std::string> readBuffers_;
};
