#pragma once

#include <cstdint>
#include <string>

struct ProcessingStats {
    std::uint64_t checksum{0};
    long long processMicros{0};
};

class DataProcessor {
public:
    DataProcessor(bool useOpenMP, int rounds);

    ProcessingStats process(const std::string& data) const;
    bool useOpenMP() const;

private:
    bool useOpenMP_{false};
    int rounds_{1};
};
