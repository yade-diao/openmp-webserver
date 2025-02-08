#pragma once

#include <condition_variable>
#include <fstream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

enum class LogMode {
    Sync,
    Async,
};

class Logger {
public:
    Logger(const std::string& filePath, LogMode mode);
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void info(const std::string& message);
    void error(const std::string& message);

private:
    void writeLine(const std::string& level, const std::string& message);
    void consumeLoop();
    static std::string nowText();

    std::ofstream file_;
    LogMode mode_;

    std::mutex ioMutex_;

    std::queue<std::string> queue_;
    std::mutex queueMutex_;
    std::condition_variable cv_;
    bool stopping_{false};
    std::thread worker_;
};
