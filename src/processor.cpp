#include "processor.h"

#include "backend_openmp.h"
#include "backend_pthread.h"
#include "backend_serial.h"

#include <sys/time.h>

namespace {
// Returns current wall clock time in microseconds.
long long now_us() {
    timeval tv;
    gettimeofday(&tv, NULL);
    return static_cast<long long>(tv.tv_sec) * 1000000LL + static_cast<long long>(tv.tv_usec);
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

    unsigned long long checksum = 0ULL;
    if (backend_ == BACKEND_SERIAL) {
        checksum = backend_serial_checksum(data, rounds_);
    } else if (backend_ == BACKEND_PTHREAD) {
        checksum = backend_pthread_checksum(data, rounds_, workers_);
    } else {
        checksum = backend_openmp_checksum(data, rounds_, workers_);
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
