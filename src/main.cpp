#include "epoll_server.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
TriggerMode parseTriggerMode(const std::string& input) {
    if (input == "lt" || input == "LT") {
        return TriggerMode::LT;
    }
    if (input == "et" || input == "ET") {
        return TriggerMode::ET;
    }
    throw std::invalid_argument("trigger mode must be lt or et");
}

EventModel parseEventModel(const std::string& input) {
    if (input == "reactor") {
        return EventModel::Reactor;
    }
    if (input == "proactor") {
        return EventModel::Proactor;
    }
    throw std::invalid_argument("event model must be reactor or proactor");
}

LogMode parseLogMode(const std::string& input) {
    if (input == "sync") {
        return LogMode::Sync;
    }
    if (input == "async") {
        return LogMode::Async;
    }
    throw std::invalid_argument("log mode must be sync or async");
}
} // namespace

int main(int argc, char** argv) {
    uint16_t port = 8080;
    std::size_t workerCount = std::thread::hardware_concurrency();
    TriggerMode triggerMode = TriggerMode::LT;
    EventModel eventModel = EventModel::Reactor;
    std::string staticRoot = "./static";
    std::string dbPath = "./webserver.db";
    std::string logPath = "./server.log";
    LogMode logMode = LogMode::Sync;

    if (workerCount == 0) {
        workerCount = 8;
    }

    if (argc >= 2) {
        port = static_cast<uint16_t>(std::atoi(argv[1]));
    }
    if (argc >= 3) {
        const int parsed = std::atoi(argv[2]);
        if (parsed > 0) {
            workerCount = static_cast<std::size_t>(parsed);
        }
    }
    if (argc >= 4) {
        triggerMode = parseTriggerMode(argv[3]);
    }
    if (argc >= 5) {
        eventModel = parseEventModel(argv[4]);
    }
    if (argc >= 6) {
        staticRoot = argv[5];
    }
    if (argc >= 7) {
        dbPath = argv[6];
    }
    if (argc >= 8) {
        logPath = argv[7];
    }
    if (argc >= 9) {
        logMode = parseLogMode(argv[8]);
    }

    try {
        EpollServer server(port, workerCount, triggerMode, eventModel, dbPath, staticRoot, logPath, logMode);
        server.run();
    } catch (const std::exception& ex) {
        std::cerr << "Fatal: " << ex.what() << "\n";
        std::cerr << "Usage: ./openmp_webserver [port] [workerCount] [lt|et] [reactor|proactor] [staticRoot] [dbPath] [logPath] [sync|async]\n";
        return 1;
    }

    return 0;
}
