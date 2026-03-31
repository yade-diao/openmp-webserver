#pragma once

#include "processor.h"

#include <pthread.h>
#include <string>
#include <vector>

struct BackgroundJob {
    unsigned long long id;
    std::string full_path;
    std::string file_name;
};

class Server {
public:
    // Builds the server with runtime paths and backend settings.
    Server(int port, const std::string& static_root, int rounds, int workers, BackendType backend);
    // Stops worker thread and releases network resources.
    ~Server();

    // Starts network loop and background worker.
    bool run();

private:
    // Creates static and upload directories when missing.
    bool init_dirs();
    // Creates and configures the listening socket.
    bool init_socket();
    // Creates epoll and registers the listen socket.
    bool init_epoll();
    // Runs the blocking epoll event loop forever.
    void event_loop();

    // Accepts all pending inbound clients.
    void accept_clients();
    // Reads one HTTP request and sends one response.
    void handle_client(int fd);

    // Routes request path to a specific handler.
    std::string route(const std::string& method, const std::string& target, const std::string& body, std::string& status, std::string& content_type);

    // Saves uploaded content and queues background processing.
    std::string handle_upload(const std::string& target, const std::string& body, std::string& status);
    // Returns file bytes or task/meta text content.
    std::string handle_get_file(const std::string& path, std::string& status, std::string& content_type);
    // Recomputes checksum using selected backend for comparison.
    std::string handle_compare(const std::string& target, std::string& status);

    // Pushes one background job into the worker queue.
    void enqueue_job(const BackgroundJob& job);
    // Adapts pthread callback to instance worker loop.
    static void* worker_entry(void* self);
    // Processes queued jobs and writes task/meta outputs.
    void worker_loop();

    // Validates a file name against path traversal input.
    static bool safe_file_name(const std::string& name);
    // Extracts one query parameter value from raw query text.
    static std::string get_query_value(const std::string& query, const std::string& key);
    // Loads binary file contents into memory.
    static bool read_binary(const std::string& path, std::string& out);
    // Writes full binary payload to a file path.
    static bool write_binary(const std::string& path, const std::string& data);
    // Formats an HTTP/1.1 response from body and headers.
    static std::string build_http(const std::string& body, const std::string& status, const std::string& content_type);

    int port_;
    int listen_fd_;
    int epoll_fd_;

    std::string static_root_;
    std::string upload_root_;

    Processor processor_;
    int rounds_;
    int workers_;

    pthread_t worker_tid_;
    pthread_mutex_t queue_mutex_;
    pthread_cond_t queue_cv_;
    bool worker_stop_;
    std::vector<BackgroundJob> queue_;
    unsigned long long next_job_id_;
};
