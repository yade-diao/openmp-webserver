#include "backend_openmp.h"

#include <cstddef>

#if defined(_OPENMP)
#include <omp.h>
#endif

namespace {
unsigned long long mix_checksum(unsigned long long state, unsigned long long input) {
    state ^= input + 0x9e3779b97f4a7c15ULL + (state << 6) + (state >> 2);
    return state;
}
}

unsigned long long backend_openmp_checksum(const std::string& data, int rounds, int workers) {
    if (workers <= 0) {
        workers = 1;
    }

    const unsigned char* ptr = data.empty() ? NULL : reinterpret_cast<const unsigned char*>(data.data());
    const std::size_t n = data.size();

#if defined(_OPENMP)
    omp_set_dynamic(0);
#endif

    unsigned long long checksum = 0ULL;
    int r = 0;
    while (r < rounds) {
        unsigned long long round_sum = 0ULL;
#if defined(_OPENMP)
        // Keep serial math unchanged; only add OpenMP parallel reduction.
#pragma omp parallel for num_threads(workers) schedule(static, 4096) reduction(+:round_sum)
        for (long long i = 0; i < static_cast<long long>(n); ++i) {
            const unsigned long long b = static_cast<unsigned long long>(ptr[static_cast<std::size_t>(i)]);
            round_sum += (b + 1ULL) * static_cast<unsigned long long>(i + 1 + r * 131);
        }
#else
        std::size_t i = 0;
        while (i < n) {
            const unsigned long long b = static_cast<unsigned long long>(ptr[i]);
            round_sum += (b + 1ULL) * static_cast<unsigned long long>(i + 1 + r * 131);
            ++i;
        }
#endif
        checksum = mix_checksum(checksum, round_sum);
        ++r;
    }

    return checksum;
}
