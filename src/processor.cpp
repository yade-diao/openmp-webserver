#include "processor.h"

#include <algorithm>
#include <pthread.h>
#include <sys/time.h>
#include <vector>

#if defined(_OPENMP)
#include <omp.h>
#endif

namespace {
// Mixes one round value into the rolling checksum.
unsigned long long mix_checksum(unsigned long long state, unsigned long long input) {
    state ^= input + 0x9e3779b97f4a7c15ULL + (state << 6) + (state >> 2);
    return state;
}

// Returns current wall clock time in microseconds.
long long now_us() {
    timeval tv;
    gettimeofday(&tv, NULL);
    return static_cast<long long>(tv.tv_sec) * 1000000LL + static_cast<long long>(tv.tv_usec);
}

struct ChunkTask {
    const unsigned char* ptr;
    std::size_t begin;
    std::size_t end;
    int round;
    unsigned long long* partial;
    int slot;
};

// Computes a partial chunk sum for the pthread backend.
void* pthread_chunk_worker(void* arg) {
    ChunkTask* task = static_cast<ChunkTask*>(arg);
    unsigned long long local = 0ULL;
    std::size_t i = task->begin;
    while (i < task->end) {
        const unsigned long long b = static_cast<unsigned long long>(task->ptr[i]);
        local += (b + 1ULL) * static_cast<unsigned long long>(i + 1 + task->round * 131);
        ++i;
    }
    task->partial[task->slot] = local;
    return NULL;
}
}

// Creates a processor and normalizes invalid runtime parameters.
Processor::Processor(int rounds, int workers, BackendType backend)
    : rounds_(rounds), workers_(workers), backend_(backend) {
    if (rounds_ <= 0) {
        rounds_ = 1;
    }
    if (workers_ <= 0) {
        workers_ = 1;
    }
#if !defined(_OPENMP)
    if (backend_ == BACKEND_OPENMP) {
        backend_ = BACKEND_PTHREAD;
    }
#endif
}

// Runs one full data processing job and returns timing plus checksum.
ProcessingStats Processor::process(const std::string& data) const {
    const long long t0 = now_us();

    const unsigned char* ptr = data.empty() ? NULL : reinterpret_cast<const unsigned char*>(data.data());
    const std::size_t n = data.size();
    unsigned long long checksum = 0ULL;

    int r = 0;
    while (r < rounds_) {
        unsigned long long round_sum = 0ULL;

#if defined(_OPENMP)
        if (backend_ == BACKEND_OPENMP && workers_ > 1) {
#pragma omp parallel for num_threads(workers_) schedule(static, 4096) reduction(+:round_sum)
            for (long long i = 0; i < static_cast<long long>(n); ++i) {
                const unsigned long long b = static_cast<unsigned long long>(ptr[static_cast<std::size_t>(i)]);
                round_sum += (b + 1ULL) * static_cast<unsigned long long>(i + 1 + r * 131);
            }
        } else
#endif
        if (backend_ == BACKEND_PTHREAD && workers_ > 1 && n > 0) {
            const int chunks = workers_;
            const std::size_t chunk_size = (n + static_cast<std::size_t>(chunks) - 1U) / static_cast<std::size_t>(chunks);
            std::vector<pthread_t> tids(static_cast<std::size_t>(chunks));
            std::vector<ChunkTask> tasks(static_cast<std::size_t>(chunks));
            std::vector<unsigned long long> partial(static_cast<std::size_t>(chunks), 0ULL);

            int launched = 0;
            int t = 0;
            while (t < chunks) {
                const std::size_t begin = static_cast<std::size_t>(t) * chunk_size;
                const std::size_t end = std::min(n, begin + chunk_size);
                tasks[static_cast<std::size_t>(t)].ptr = ptr;
                tasks[static_cast<std::size_t>(t)].begin = begin;
                tasks[static_cast<std::size_t>(t)].end = end;
                tasks[static_cast<std::size_t>(t)].round = r;
                tasks[static_cast<std::size_t>(t)].partial = &partial[0];
                tasks[static_cast<std::size_t>(t)].slot = t;
                if (begin < end) {
                    pthread_create(&tids[static_cast<std::size_t>(t)], NULL, &pthread_chunk_worker, &tasks[static_cast<std::size_t>(t)]);
                    ++launched;
                }
                ++t;
            }

            t = 0;
            while (t < chunks) {
                const std::size_t begin = static_cast<std::size_t>(t) * chunk_size;
                const std::size_t end = std::min(n, begin + chunk_size);
                if (begin < end) {
                    pthread_join(tids[static_cast<std::size_t>(t)], NULL);
                }
                ++t;
            }

            (void)launched;
            t = 0;
            while (t < chunks) {
                round_sum += partial[static_cast<std::size_t>(t)];
                ++t;
            }
        } else {
            std::size_t i = 0;
            while (i < n) {
                const unsigned long long b = static_cast<unsigned long long>(ptr[i]);
                round_sum += (b + 1ULL) * static_cast<unsigned long long>(i + 1 + r * 131);
                ++i;
            }
        }

        checksum = mix_checksum(checksum, round_sum);
        ++r;
    }

    ProcessingStats out;
    out.checksum = checksum;
    out.process_us = now_us() - t0;
    return out;
}

// Returns a printable backend label.
const char* Processor::backendName() const {
    if (backend_ == BACKEND_OPENMP) {
        return "openmp";
    }
    if (backend_ == BACKEND_PTHREAD) {
        return "pthread";
    }
    return "serial";
}

// Returns configured worker count.
int Processor::workers() const {
    return workers_;
}
