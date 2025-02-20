#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "thread_pool.h"

enum class ComputeBackend {
    OpenMP,
    StdQueue,
};

struct ProcessingStats {
    std::uint64_t checksum{0};
    long long processMicros{0};
};

class DataProcessor {
public:
    DataProcessor(int rounds, std::size_t workerThreads, ComputeBackend backend);

    ProcessingStats process(const std::string& data) const;
    std::size_t workerThreads() const;
    ComputeBackend backend() const;
    const char* backendName() const;

private:
    int rounds_{1};
    std::size_t workerThreads_{1};
    ComputeBackend backend_{ComputeBackend::OpenMP};
    std::unique_ptr<ThreadPool> stdQueuePool_;
    std::size_t minBytesForOMP_{64 * 1024};
    std::uint64_t minWorkForOMP_{32ULL * 1024ULL * 1024ULL};
};
