#include "data_processor.h"

#include <algorithm>
#include <chrono>

#if defined(_OPENMP)
#include <omp.h>
#endif

namespace {
std::uint64_t mixChecksum(std::uint64_t state, std::uint64_t input) {
    state ^= input + 0x9e3779b97f4a7c15ULL + (state << 6) + (state >> 2);
    return state;
}
} // namespace

DataProcessor::DataProcessor(bool useOpenMP, int rounds) {
#if defined(_OPENMP)
    useOpenMP_ = useOpenMP;
#else
    useOpenMP_ = false;
#endif
    rounds_ = std::max(1, rounds);
}

ProcessingStats DataProcessor::process(const std::string& data) const {
    const auto start = std::chrono::high_resolution_clock::now();

    std::uint64_t checksum = 0;
    const std::size_t n = data.size();

    for (int r = 0; r < rounds_; ++r) {
        std::uint64_t roundSum = 0;

#if defined(_OPENMP)
        if (useOpenMP_) {
#pragma omp parallel for reduction(+:roundSum)
            for (long long i = 0; i < static_cast<long long>(n); ++i) {
                const std::uint64_t b = static_cast<unsigned char>(data[static_cast<std::size_t>(i)]);
                roundSum += (b + 1ULL) * static_cast<std::uint64_t>(i + 1 + r * 131);
            }
        } else {
            for (std::size_t i = 0; i < n; ++i) {
                const std::uint64_t b = static_cast<unsigned char>(data[i]);
                roundSum += (b + 1ULL) * static_cast<std::uint64_t>(i + 1 + r * 131);
            }
        }
#else
        for (std::size_t i = 0; i < n; ++i) {
            const std::uint64_t b = static_cast<unsigned char>(data[i]);
            roundSum += (b + 1ULL) * static_cast<std::uint64_t>(i + 1 + r * 131);
        }
#endif

        checksum = mixChecksum(checksum, roundSum);
    }

    const auto end = std::chrono::high_resolution_clock::now();
    const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    return ProcessingStats{checksum, micros};
}

bool DataProcessor::useOpenMP() const {
    return useOpenMP_;
}
