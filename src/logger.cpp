#include "logger.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

Logger::Logger(const std::string& filePath, LogMode mode) : mode_(mode) {
    file_.open(filePath, std::ios::app);
    if (!file_.is_open()) {
        throw std::runtime_error("failed to open log file: " + filePath);
    }

    if (mode_ == LogMode::Async) {
        worker_ = std::thread([this]() { consumeLoop(); });
    }
}

Logger::~Logger() {
    if (mode_ == LogMode::Async) {
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            stopping_ = true;
        }
        cv_.notify_all();
        if (worker_.joinable()) {
            worker_.join();
        }
    }
}

void Logger::info(const std::string& message) {
    writeLine("INFO", message);
}

void Logger::error(const std::string& message) {
    writeLine("ERROR", message);
}

void Logger::writeLine(const std::string& level, const std::string& message) {
    const std::string line = nowText() + " [" + level + "] " + message;

    if (mode_ == LogMode::Sync) {
        std::lock_guard<std::mutex> lock(ioMutex_);
        file_ << line << '\n';
        file_.flush();
        std::cout << line << '\n';
        return;
    }

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        queue_.push(line);
    }
    cv_.notify_one();
}

void Logger::consumeLoop() {
    for (;;) {
        std::string line;

        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            cv_.wait(lock, [this]() { return stopping_ || !queue_.empty(); });
            if (stopping_ && queue_.empty()) {
                return;
            }
            line = std::move(queue_.front());
            queue_.pop();
        }

        std::lock_guard<std::mutex> lock(ioMutex_);
        file_ << line << '\n';
        file_.flush();
        std::cout << line << '\n';
    }
}

std::string Logger::nowText() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}
