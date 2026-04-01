#include "backend_serial.h"

#include <cstddef>

namespace {
unsigned long long mix_checksum(unsigned long long state, unsigned long long input) {
    state ^= input + 0x9e3779b97f4a7c15ULL + (state << 6) + (state >> 2);
    return state;
}
}

unsigned long long backend_serial_checksum(const std::string& data, int rounds) {
    const unsigned char* ptr = data.empty() ? NULL : reinterpret_cast<const unsigned char*>(data.data());
    const std::size_t n = data.size();

    unsigned long long checksum = 0ULL;
    int r = 0;
    while (r < rounds) {
        unsigned long long round_sum = 0ULL;
        std::size_t i = 0;
        while (i < n) {
            const unsigned long long b = static_cast<unsigned long long>(ptr[i]);
            round_sum += (b + 1ULL) * static_cast<unsigned long long>(i + 1 + r * 131);
            ++i;
        }
        checksum = mix_checksum(checksum, round_sum);
        ++r;
    }

    return checksum;
}
