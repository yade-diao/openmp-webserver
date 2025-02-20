#include "data_processor.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <numeric>
#include <vector>

#if defined(_OPENMP)
#include <omp.h>
#endif

namespace {
std::uint64_t mixChecksum(std::uint64_t state, std::uint64_t input) {
    state ^= input + 0x9e3779b97f4a7c15ULL + (state << 6) + (state >> 2);
    return state;
}
} // namespace

DataProcessor::DataProcessor(int rounds, std::size_t workerThreads, ComputeBackend backend) {
    rounds_ = std::max(1, rounds);
    workerThreads_ = std::max<std::size_t>(1, workerThreads);
    backend_ = backend;

    if (backend_ == ComputeBackend::StdQueue && workerThreads_ > 1) {
        stdQueuePool_ = std::make_unique<ThreadPool>(workerThreads_);
    }

#if defined(_OPENMP)
    omp_set_dynamic(0);
#else
    if (backend_ == ComputeBackend::OpenMP) {
        backend_ = ComputeBackend::StdQueue;
    }
#endif
}

ProcessingStats DataProcessor::process(const std::string& data) const {
    const auto start = std::chrono::high_resolution_clock::now();

    std::uint64_t checksum = 0;
    const std::size_t n = data.size();
    const std::uint64_t work = static_cast<std::uint64_t>(n) * static_cast<std::uint64_t>(rounds_);

    const bool runParallel = workerThreads_ > 1 && n >= minBytesForOMP_ && work >= minWorkForOMP_;

    const unsigned char* ptr = reinterpret_cast<const unsigned char*>(data.data());

    for (int r = 0; r < rounds_; ++r) {
        std::uint64_t roundSum = 0;

#if defined(_OPENMP)
        if (runParallel && backend_ == ComputeBackend::OpenMP) {
#pragma omp parallel for num_threads(workerThreads_) schedule(static, 4096) reduction(+:roundSum)
            for (long long i = 0; i < static_cast<long long>(n); ++i) {
                const std::uint64_t b = static_cast<std::uint64_t>(ptr[static_cast<std::size_t>(i)]);
                roundSum += (b + 1ULL) * static_cast<std::uint64_t>(i + 1 + r * 131);
            }
        } else
#endif
        if (runParallel && backend_ == ComputeBackend::StdQueue && stdQueuePool_) {
            const std::size_t chunks = workerThreads_;
            const std::size_t chunkSize = (n + chunks - 1) / chunks;
            std::vector<std::uint64_t> partial(chunks, 0);

            std::mutex doneMutex;
            std::condition_variable doneCv;
            std::size_t doneCount = 0;

            for (std::size_t t = 0; t < chunks; ++t) {
                const std::size_t begin = t * chunkSize;
                const std::size_t end = std::min(n, begin + chunkSize);

                if (begin >= end) {
                    std::lock_guard<std::mutex> lock(doneMutex);
                    ++doneCount;
                    continue;
                }

                stdQueuePool_->enqueue([&, t, begin, end, r]() {
                    std::uint64_t local = 0;
                    for (std::size_t i = begin; i < end; ++i) {
                        const std::uint64_t b = static_cast<std::uint64_t>(ptr[i]);
                        local += (b + 1ULL) * static_cast<std::uint64_t>(i + 1 + r * 131);
                    }
                    partial[t] = local;

                    {
                        std::lock_guard<std::mutex> lock(doneMutex);
                        ++doneCount;
                    }
                    doneCv.notify_one();
                });
            }

            {
                std::unique_lock<std::mutex> lock(doneMutex);
                doneCv.wait(lock, [&]() { return doneCount == chunks; });
            }

            roundSum = std::accumulate(partial.begin(), partial.end(), static_cast<std::uint64_t>(0));
        } else {
            for (std::size_t i = 0; i < n; ++i) {
                const std::uint64_t b = static_cast<std::uint64_t>(ptr[i]);
                roundSum += (b + 1ULL) * static_cast<std::uint64_t>(i + 1 + r * 131);
            }
        }

        checksum = mixChecksum(checksum, roundSum);
    }

    const auto end = std::chrono::high_resolution_clock::now();
    const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    return ProcessingStats{checksum, micros};
}

std::size_t DataProcessor::workerThreads() const {
    return workerThreads_;
}

ComputeBackend DataProcessor::backend() const {
    return backend_;
}

const char* DataProcessor::backendName() const {
    return backend_ == ComputeBackend::OpenMP ? "openmp" : "std";
}
