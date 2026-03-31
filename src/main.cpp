#include "server.h"

#include <cstdlib>
#include <iostream>
#include <string>

// Parses startup arguments and runs the HTTP server.
int main(int argc, char** argv) {
    int port = 8080;
    std::string staticRoot = "./static";
    int rounds = 64;
    int workers = 4;
    BackendType backend = BACKEND_OPENMP;

    if (argc >= 2) {
        port = std::atoi(argv[1]);
        if (port <= 0) {
            port = 8080;
        }
    }
    if (argc >= 3) {
        staticRoot = argv[2];
    }
    if (argc >= 4) {
        rounds = std::atoi(argv[3]);
        if (rounds <= 0) {
            rounds = 64;
        }
    }
    if (argc >= 5) {
        workers = std::atoi(argv[4]);
        if (workers <= 0) {
            workers = 4;
        }
    }
    if (argc >= 6) {
        const std::string b = argv[5];
        if (b == "serial") {
            backend = BACKEND_SERIAL;
        } else if (b == "pthread") {
            backend = BACKEND_PTHREAD;
        } else {
            backend = BACKEND_OPENMP;
        }
    }

    Server server(port, staticRoot, rounds, workers, backend);
    if (!server.run()) {
        std::cerr << "Usage: ./openmp_webserver [port] [staticRoot] [rounds] [workers] [openmp|pthread|serial]\n";
        return 1;
    }
    return 0;
}
