#pragma once

#include <string>

// Selects the compute backend used for background processing.
enum BackendType {
    BACKEND_SERIAL,
    BACKEND_PTHREAD,
    BACKEND_OPENMP,
};

// Stores checksum and elapsed processing time.
struct ProcessingStats {
    unsigned long long checksum;
    long long process_us;
};

// Runs hash-style compute work with serial, pthread, or OpenMP backend.
class Processor {
public:
    // Initializes processor settings and backend fallback behavior.
    Processor(int rounds, int workers, BackendType backend);

    // Processes data and returns checksum plus elapsed microseconds.
    ProcessingStats process(const std::string& data) const;
    // Returns current backend name.
    const char* backendName() const;
    // Returns configured worker count.
    int workers() const;

private:
    int rounds_;
    int workers_;
    BackendType backend_;
};
