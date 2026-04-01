#include "backend_pthread.h"

#include <algorithm>
#include <cstddef>
#include <pthread.h>
#include <vector>

namespace {
unsigned long long mix_checksum(unsigned long long state, unsigned long long input) {
    state ^= input + 0x9e3779b97f4a7c15ULL + (state << 6) + (state >> 2);
    return state;
}

struct ChunkTask {
    const unsigned char* ptr;
    std::size_t begin;
    std::size_t end;
    int round;
    unsigned long long* partial;
    int slot;
};

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

unsigned long long backend_pthread_checksum(const std::string& data, int rounds, int workers) {
    if (workers <= 0) {
        workers = 1;
    }

    const unsigned char* ptr = data.empty() ? NULL : reinterpret_cast<const unsigned char*>(data.data());
    const std::size_t n = data.size();

    unsigned long long checksum = 0ULL;
    int r = 0;
    while (r < rounds) {
        unsigned long long round_sum = 0ULL;

        if (workers > 1 && n > 0) {
            const int chunks = workers;
            const std::size_t chunk_size = (n + static_cast<std::size_t>(chunks) - 1U) / static_cast<std::size_t>(chunks);
            std::vector<pthread_t> tids(static_cast<std::size_t>(chunks));
            std::vector<ChunkTask> tasks(static_cast<std::size_t>(chunks));
            std::vector<unsigned long long> partial(static_cast<std::size_t>(chunks), 0ULL);

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

    return checksum;
}
