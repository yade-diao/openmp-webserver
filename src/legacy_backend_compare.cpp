#include <pthread.h>
#include <sys/time.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#if defined(_OPENMP)
#include <omp.h>
#endif

namespace {
unsigned long long mix_checksum(unsigned long long state, unsigned long long input) {
    state ^= input + 0x9e3779b97f4a7c15ULL + (state << 6) + (state >> 2);
    return state;
}

long long now_us() {
    timeval tv;
    gettimeofday(&tv, NULL);
    return static_cast<long long>(tv.tv_sec) * 1000000LL + static_cast<long long>(tv.tv_usec);
}

struct WorkItem {
    const unsigned char* ptr;
    std::size_t begin;
    std::size_t end;
    int round;
    unsigned long long* partial;
    std::size_t slot;
};

struct LegacyQueueProcessor {
    explicit LegacyQueueProcessor(std::size_t worker_count)
        : worker_count_(worker_count),
          head_(0),
          tail_(0),
          count_(0),
          stopping_(false),
          pending_(0) {
        if (worker_count_ == 0) {
            worker_count_ = 1;
        }
        queue_.resize(worker_count_ * 4 + 16);
        workers_.resize(worker_count_);

        pthread_mutex_init(&queue_mutex_, NULL);
        pthread_cond_init(&queue_cv_, NULL);
        pthread_mutex_init(&done_mutex_, NULL);
        pthread_cond_init(&done_cv_, NULL);

        for (std::size_t i = 0; i < worker_count_; ++i) {
            pthread_create(&workers_[i], NULL, &LegacyQueueProcessor::worker_entry, this);
        }
    }

    ~LegacyQueueProcessor() {
        pthread_mutex_lock(&queue_mutex_);
        stopping_ = true;
        pthread_cond_broadcast(&queue_cv_);
        pthread_mutex_unlock(&queue_mutex_);

        for (std::size_t i = 0; i < workers_.size(); ++i) {
            pthread_join(workers_[i], NULL);
        }

        pthread_cond_destroy(&done_cv_);
        pthread_mutex_destroy(&done_mutex_);
        pthread_cond_destroy(&queue_cv_);
        pthread_mutex_destroy(&queue_mutex_);
    }

    unsigned long long run(const std::vector<unsigned char>& data, int rounds) {
        const std::size_t n = data.size();
        const unsigned char* ptr = data.empty() ? NULL : &data[0];
        const std::size_t chunks = worker_count_;
        const std::size_t chunk_size = (n + chunks - 1) / chunks;

        std::vector<unsigned long long> partial(chunks, 0ULL);
        unsigned long long checksum = 0ULL;

        for (int r = 0; r < rounds; ++r) {
            std::fill(partial.begin(), partial.end(), 0ULL);

            std::size_t tasks = 0;
            pthread_mutex_lock(&done_mutex_);
            pending_ = 0;
            pthread_mutex_unlock(&done_mutex_);

            for (std::size_t t = 0; t < chunks; ++t) {
                const std::size_t begin = t * chunk_size;
                const std::size_t end = std::min(n, begin + chunk_size);
                if (begin >= end) {
                    continue;
                }

                WorkItem item;
                item.ptr = ptr;
                item.begin = begin;
                item.end = end;
                item.round = r;
                item.partial = &partial[0];
                item.slot = t;

                pthread_mutex_lock(&done_mutex_);
                ++pending_;
                pthread_mutex_unlock(&done_mutex_);

                push(item);
                ++tasks;
            }

            if (tasks > 0) {
                pthread_mutex_lock(&done_mutex_);
                while (pending_ > 0) {
                    pthread_cond_wait(&done_cv_, &done_mutex_);
                }
                pthread_mutex_unlock(&done_mutex_);
            }

            unsigned long long round_sum = 0ULL;
            for (std::size_t i = 0; i < partial.size(); ++i) {
                round_sum += partial[i];
            }
            checksum = mix_checksum(checksum, round_sum);
        }

        return checksum;
    }

private:
    static void* worker_entry(void* self) {
        LegacyQueueProcessor* p = static_cast<LegacyQueueProcessor*>(self);
        p->worker_loop();
        return NULL;
    }

    void push(const WorkItem& item) {
        pthread_mutex_lock(&queue_mutex_);
        while (count_ == queue_.size() && !stopping_) {
            pthread_cond_wait(&queue_cv_, &queue_mutex_);
        }

        if (!stopping_) {
            queue_[tail_] = item;
            tail_ = (tail_ + 1) % queue_.size();
            ++count_;
            pthread_cond_signal(&queue_cv_);
        }
        pthread_mutex_unlock(&queue_mutex_);
    }

    bool pop(WorkItem& out) {
        pthread_mutex_lock(&queue_mutex_);
        while (count_ == 0 && !stopping_) {
            pthread_cond_wait(&queue_cv_, &queue_mutex_);
        }

        if (count_ == 0 && stopping_) {
            pthread_mutex_unlock(&queue_mutex_);
            return false;
        }

        out = queue_[head_];
        head_ = (head_ + 1) % queue_.size();
        --count_;
        pthread_cond_signal(&queue_cv_);
        pthread_mutex_unlock(&queue_mutex_);
        return true;
    }

    void worker_loop() {
        for (;;) {
            WorkItem item;
            if (!pop(item)) {
                return;
            }

            unsigned long long local = 0ULL;
            for (std::size_t i = item.begin; i < item.end; ++i) {
                const unsigned long long b = static_cast<unsigned long long>(item.ptr[i]);
                local += (b + 1ULL) * static_cast<unsigned long long>(i + 1 + item.round * 131);
            }
            item.partial[item.slot] = local;

            pthread_mutex_lock(&done_mutex_);
            if (pending_ > 0) {
                --pending_;
            }
            if (pending_ == 0) {
                pthread_cond_signal(&done_cv_);
            }
            pthread_mutex_unlock(&done_mutex_);
        }
    }

    std::size_t worker_count_;
    std::vector<pthread_t> workers_;
    std::vector<WorkItem> queue_;
    std::size_t head_;
    std::size_t tail_;
    std::size_t count_;
    bool stopping_;

    pthread_mutex_t queue_mutex_;
    pthread_cond_t queue_cv_;

    std::size_t pending_;
    pthread_mutex_t done_mutex_;
    pthread_cond_t done_cv_;
};

unsigned long long run_serial(const std::vector<unsigned char>& data, int rounds) {
    const std::size_t n = data.size();
    const unsigned char* ptr = data.empty() ? NULL : &data[0];
    unsigned long long checksum = 0ULL;
    for (int r = 0; r < rounds; ++r) {
        unsigned long long round_sum = 0ULL;
        for (std::size_t i = 0; i < n; ++i) {
            const unsigned long long b = static_cast<unsigned long long>(ptr[i]);
            round_sum += (b + 1ULL) * static_cast<unsigned long long>(i + 1 + r * 131);
        }
        checksum = mix_checksum(checksum, round_sum);
    }
    return checksum;
}

unsigned long long run_openmp(const std::vector<unsigned char>& data, int rounds, std::size_t omp_threads) {
    const std::size_t n = data.size();
    const unsigned char* ptr = data.empty() ? NULL : &data[0];
    unsigned long long checksum = 0ULL;

#if defined(_OPENMP)
    omp_set_dynamic(0);
#endif

    for (int r = 0; r < rounds; ++r) {
        unsigned long long round_sum = 0ULL;
#if defined(_OPENMP)
#pragma omp parallel for num_threads(omp_threads) schedule(static, 4096) reduction(+:round_sum)
        for (long long i = 0; i < static_cast<long long>(n); ++i) {
            const unsigned long long b = static_cast<unsigned long long>(ptr[static_cast<std::size_t>(i)]);
            round_sum += (b + 1ULL) * static_cast<unsigned long long>(i + 1 + r * 131);
        }
#else
        for (std::size_t i = 0; i < n; ++i) {
            const unsigned long long b = static_cast<unsigned long long>(ptr[i]);
            round_sum += (b + 1ULL) * static_cast<unsigned long long>(i + 1 + r * 131);
        }
#endif
        checksum = mix_checksum(checksum, round_sum);
    }
    return checksum;
}
} // namespace

int main(int argc, char** argv) {
    int data_mb = 8;
    int rounds = 1024;
    int workers = 4;

    if (argc >= 2) {
        data_mb = std::max(1, std::atoi(argv[1]));
    }
    if (argc >= 3) {
        rounds = std::max(1, std::atoi(argv[2]));
    }
    if (argc >= 4) {
        workers = std::max(1, std::atoi(argv[3]));
    }

    const std::size_t total = static_cast<std::size_t>(data_mb) * 1024ULL * 1024ULL;
    std::vector<unsigned char> data(total, 'a');

    const long long t0 = now_us();
    const unsigned long long c0 = run_serial(data, rounds);
    const long long t1 = now_us();

    LegacyQueueProcessor legacy(static_cast<std::size_t>(workers));
    const long long t2 = now_us();
    const unsigned long long c1 = legacy.run(data, rounds);
    const long long t3 = now_us();

    const long long t4 = now_us();
    const unsigned long long c2 = run_openmp(data, rounds, static_cast<std::size_t>(workers));
    const long long t5 = now_us();

    const long long serial_us = t1 - t0;
    const long long pthread_us = t3 - t2;
    const long long openmp_us = t5 - t4;

    std::printf("data_mb=%d rounds=%d workers=%d\n", data_mb, rounds, workers);
    std::printf("serial_us=%lld checksum=%llu\n", serial_us, c0);
    std::printf("legacy_pthread_queue_us=%lld checksum=%llu\n", pthread_us, c1);
    std::printf("openmp_us=%lld checksum=%llu\n", openmp_us, c2);

    if (pthread_us > 0) {
        std::printf("speedup_pthread_vs_serial=%.4f\n", static_cast<double>(serial_us) / static_cast<double>(pthread_us));
    }
    if (openmp_us > 0) {
        std::printf("speedup_openmp_vs_serial=%.4f\n", static_cast<double>(serial_us) / static_cast<double>(openmp_us));
        std::printf("speedup_openmp_vs_pthread=%.4f\n", static_cast<double>(pthread_us) / static_cast<double>(openmp_us));
    }

    return 0;
}
