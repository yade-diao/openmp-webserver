#include "server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace {
const int MAX_EVENTS = 256;
const int BUF_SIZE = 4096;
const char kEncMagic[] = "ENC1";
const char kXorKey[] = "openmp-webserver-demo-key";

// Sets a socket file descriptor to non-blocking mode.
bool set_non_blocking(int fd) {
    int old_flags = fcntl(fd, F_GETFL, 0);
    if (old_flags < 0) {
        return false;
    }
    if (fcntl(fd, F_SETFL, old_flags | O_NONBLOCK) < 0) {
        return false;
    }
    return true;
}

// Splits request target into path and query parts.
std::string split_path(const std::string& target, std::string& query) {
    std::string::size_type q = target.find('?');
    if (q == std::string::npos) {
        query.clear();
        return target;
    }
    query = target.substr(q + 1);
    return target.substr(0, q);
}

// Parses checksum value from metadata text.
unsigned long long parse_checksum(const std::string& meta) {
    std::string key = "checksum=";
    std::string::size_type p = meta.find(key);
    if (p == std::string::npos) {
        return 0ULL;
    }
    std::string::size_type e = meta.find('\n', p);
    std::string v = (e == std::string::npos) ? meta.substr(p + key.size()) : meta.substr(p + key.size(), e - (p + key.size()));
    return static_cast<unsigned long long>(strtoull(v.c_str(), NULL, 10));
}

// Applies a simple XOR stream transform for demo encryption/decryption.
std::string xor_transform(const std::string& input) {
    std::string out = input;
    const std::size_t key_len = sizeof(kXorKey) - 1U;
    if (key_len == 0) {
        return out;
    }
    std::size_t i = 0;
    while (i < out.size()) {
        out[i] = static_cast<char>(static_cast<unsigned char>(out[i]) ^ static_cast<unsigned char>(kXorKey[i % key_len]));
        ++i;
    }
    return out;
}

// Returns true if payload starts with the encryption marker.
bool is_encrypted_payload(const std::string& payload) {
    return payload.size() >= 4U && payload[0] == kEncMagic[0] && payload[1] == kEncMagic[1] && payload[2] == kEncMagic[2] && payload[3] == kEncMagic[3];
}

// Prefixes marker and encrypts data for on-disk storage.
std::string encrypt_payload(const std::string& plain) {
    std::string out;
    out.reserve(4U + plain.size());
    out.append(kEncMagic, 4U);
    out += xor_transform(plain);
    return out;
}

// Decrypts payload if marker exists, otherwise returns original bytes.
std::string decrypt_payload_if_needed(const std::string& stored) {
    if (!is_encrypted_payload(stored)) {
        return stored;
    }
    return xor_transform(stored.substr(4U));
}
}

// Builds the server with filesystem roots and backend processor.
Server::Server(int port, const std::string& static_root, int rounds, int workers, BackendType backend)
    : port_(port),
      listen_fd_(-1),
      epoll_fd_(-1),
      static_root_(static_root),
      upload_root_(static_root + "/uploads"),
      processor_(rounds, workers, backend),
      rounds_(rounds),
      workers_(workers),
      worker_tid_(),
      worker_stop_(false),
      next_job_id_(1ULL) {
    pthread_mutex_init(&queue_mutex_, NULL);
    pthread_cond_init(&queue_cv_, NULL);
}

// Stops worker thread and closes all open descriptors.
Server::~Server() {
    pthread_mutex_lock(&queue_mutex_);
    worker_stop_ = true;
    pthread_cond_signal(&queue_cv_);
    pthread_mutex_unlock(&queue_mutex_);

    if (worker_tid_) {
        pthread_join(worker_tid_, NULL);
    }

    if (listen_fd_ >= 0) {
        close(listen_fd_);
    }
    if (epoll_fd_ >= 0) {
        close(epoll_fd_);
    }

    pthread_cond_destroy(&queue_cv_);
    pthread_mutex_destroy(&queue_mutex_);
}

// Initializes resources and enters the epoll processing loop.
bool Server::run() {
    if (!init_dirs()) {
        return false;
    }
    if (!init_socket()) {
        return false;
    }
    if (!init_epoll()) {
        return false;
    }
    pthread_create(&worker_tid_, NULL, &Server::worker_entry, this);

        printf("server listening on 0.0.0.0:%d backend=%s rounds=%d workers=%d\n",
            port_, processor_.backendName(), rounds_, workers_);
    event_loop();
    return true;
}

// Ensures static and upload directories exist.
bool Server::init_dirs() {
    mkdir(static_root_.c_str(), 0755);
    mkdir(upload_root_.c_str(), 0755);
    return true;
}

// Creates a non-blocking TCP listening socket.
bool Server::init_socket() {
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        return false;
    }
    int reuse = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    if (!set_non_blocking(listen_fd_)) {
        return false;
    }

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<unsigned short>(port_));

    if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        return false;
    }
    if (listen(listen_fd_, SOMAXCONN) < 0) {
        return false;
    }
    return true;
}

// Creates epoll and adds the listen socket.
bool Server::init_epoll() {
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ < 0) {
        return false;
    }
    epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.data.fd = listen_fd_;
    ev.events = EPOLLIN;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, listen_fd_, &ev) < 0) {
        return false;
    }
    return true;
}

// Waits for epoll events and dispatches handlers.
void Server::event_loop() {
    epoll_event events[MAX_EVENTS];
    while (1) {
        int ready = epoll_wait(epoll_fd_, events, MAX_EVENTS, -1);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            return;
        }
        int i = 0;
        while (i < ready) {
            int fd = events[i].data.fd;
            if (fd == listen_fd_) {
                accept_clients();
            } else {
                handle_client(fd);
            }
            ++i;
        }
    }
}

// Accepts all currently pending client connections.
void Server::accept_clients() {
    while (1) {
        sockaddr_in caddr;
        socklen_t len = sizeof(caddr);
        int cfd = accept(listen_fd_, reinterpret_cast<sockaddr*>(&caddr), &len);
        if (cfd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            }
            if (errno == EINTR) {
                continue;
            }
            return;
        }
        set_non_blocking(cfd);
        epoll_event ev;
        memset(&ev, 0, sizeof(ev));
        ev.data.fd = cfd;
        ev.events = EPOLLIN | EPOLLRDHUP;
        epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, cfd, &ev);
    }
}

// Reads a single HTTP request and writes one HTTP response.
void Server::handle_client(int fd) {
    char buf[BUF_SIZE];
    std::string req;
    while (1) {
        int n = recv(fd, buf, sizeof(buf), 0);
        if (n > 0) {
            req.append(buf, buf + n);
            if (n < static_cast<int>(sizeof(buf))) {
                break;
            }
            continue;
        }
        if (n == 0) {
            close(fd);
            return;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break;
        }
        if (errno == EINTR) {
            continue;
        }
        close(fd);
        return;
    }

    std::string::size_type p = req.find("\r\n");
    if (p == std::string::npos) {
        close(fd);
        return;
    }
    std::string first = req.substr(0, p);
    std::istringstream ss(first);
    std::string method;
    std::string target;
    std::string version;
    ss >> method >> target >> version;

    std::string body;
    std::string::size_type h = req.find("\r\n\r\n");
    if (h != std::string::npos) {
        body = req.substr(h + 4);
    }

    std::string status = "200 OK";
    std::string content_type = "text/plain; charset=utf-8";
    std::string out = route(method, target, body, status, content_type);
    std::string resp = build_http(out, status, content_type);

    send(fd, resp.data(), static_cast<int>(resp.size()), 0);
    shutdown(fd, SHUT_RDWR);
    close(fd);
}

// Routes HTTP requests to upload, files, or compare handlers.
std::string Server::route(const std::string& method,
                                const std::string& target,
                                const std::string& body,
                                std::string& status,
                                std::string& content_type) {
    std::string query;
    std::string path = split_path(target, query);

    if (path == "/upload" && (method == "POST" || method == "GET")) {
        return handle_upload(target, body, status);
    }
    if (path.find("/files/") == 0 && method == "GET") {
        return handle_get_file(path, status, content_type);
    }
    if (path == "/compare" && method == "GET") {
        return handle_compare(target, status);
    }

    status = "404 Not Found";
    return "Not Found\n";
}

// Stores upload bytes and submits a background processing job.
std::string Server::handle_upload(const std::string& target, const std::string& body, std::string& status) {
    std::string query;
    split_path(target, query);

    std::string file_name = get_query_value(query, "name");
    if (!safe_file_name(file_name)) {
        status = "400 Bad Request";
        return "invalid file name\n";
    }

    std::string content = body;
    if (content.empty()) {
        content = get_query_value(query, "content");
    }

    const std::string full = upload_root_ + "/" + file_name;
    if (!write_binary(full, content)) {
        status = "500 Internal Server Error";
        return "upload failed\n";
    }

    BackgroundJob job;
    job.id = next_job_id_++;
    job.full_path = full;
    job.file_name = file_name;
    enqueue_job(job);

    std::ostringstream out;
    out << "upload success: /files/" << file_name << "\n";
    out << "backend=" << processor_.backendName() << " job_id=" << job.id << "\n";
    out << "task status file: /files/" << file_name << ".task\n";
    return out.str();
}

// Returns uploaded file bytes or task/meta text.
std::string Server::handle_get_file(const std::string& path, std::string& status, std::string& content_type) {
    const std::string file_name = path.substr(std::string("/files/").size());
    if (!safe_file_name(file_name) && file_name.find(".task") == std::string::npos && file_name.find(".meta") == std::string::npos) {
        status = "400 Bad Request";
        return "invalid file name\n";
    }

    std::string data;
    if (!read_binary(upload_root_ + "/" + file_name, data)) {
        status = "404 Not Found";
        return "file not found\n";
    }

    content_type = "application/octet-stream";
    if (file_name.find(".meta") != std::string::npos || file_name.find(".task") != std::string::npos) {
        content_type = "text/plain; charset=utf-8";
        return data;
    }

    return decrypt_payload_if_needed(data);
}

// Re-runs processing on a file and compares stored checksum.
std::string Server::handle_compare(const std::string& target, std::string& status) {
    std::string query;
    split_path(target, query);

    std::string file_name = get_query_value(query, "name");
    std::string backend = get_query_value(query, "backend");
    if (!safe_file_name(file_name)) {
        status = "400 Bad Request";
        return "invalid file name\n";
    }

    BackendType b = BACKEND_OPENMP;
    if (backend == "serial") {
        b = BACKEND_SERIAL;
    } else if (backend == "pthread") {
        b = BACKEND_PTHREAD;
    }

    std::string data;
    if (!read_binary(upload_root_ + "/" + file_name, data)) {
        status = "404 Not Found";
        return "file not found\n";
    }

    data = decrypt_payload_if_needed(data);

    Processor p(rounds_, workers_, b);
    ProcessingStats s = p.process(data);

    std::string meta;
    unsigned long long old_checksum = 0ULL;
    if (read_binary(upload_root_ + "/" + file_name + ".meta", meta)) {
        old_checksum = parse_checksum(meta);
    }

    std::ostringstream out;
    out << "backend=" << p.backendName() << "\n";
    out << "checksum=" << s.checksum << "\n";
    out << "process_us=" << s.process_us << "\n";
    out << "meta_checksum=" << old_checksum << "\n";
    out << "match=" << (old_checksum == s.checksum ? 1 : 0) << "\n";
    return out.str();
}

// Adds one job to queue and marks task status as queued.
void Server::enqueue_job(const BackgroundJob& job) {
    const std::string task_path = job.full_path + ".task";
    write_binary(task_path, "status=queued\nbackend=" + std::string(processor_.backendName()) + "\n");

    pthread_mutex_lock(&queue_mutex_);
    queue_.push_back(job);
    pthread_cond_signal(&queue_cv_);
    pthread_mutex_unlock(&queue_mutex_);
}

// Bridges pthread callback entry to instance worker loop.
void* Server::worker_entry(void* self) {
    Server* p = static_cast<Server*>(self);
    p->worker_loop();
    return NULL;
}

// Consumes queued jobs and writes task/meta output files.
void Server::worker_loop() {
    while (1) {
        BackgroundJob job;
        bool has_job = false;

        pthread_mutex_lock(&queue_mutex_);
        while (!worker_stop_ && queue_.empty()) {
            pthread_cond_wait(&queue_cv_, &queue_mutex_);
        }
        if (worker_stop_ && queue_.empty()) {
            pthread_mutex_unlock(&queue_mutex_);
            return;
        }
        job = queue_.front();
        queue_.erase(queue_.begin());
        has_job = true;
        pthread_mutex_unlock(&queue_mutex_);

        if (!has_job) {
            continue;
        }

        const std::string task_path = job.full_path + ".task";
        {
            std::ostringstream running;
            running << "status=running\n";
            running << "job_id=" << job.id << "\n";
            running << "backend=" << processor_.backendName() << "\n";
            write_binary(task_path, running.str());
        }

        std::string data;
        if (!read_binary(job.full_path, data)) {
            write_binary(task_path, "status=failed\nreason=read_failed\n");
            continue;
        }

        ProcessingStats s = processor_.process(data);

        const std::string encrypted_data = encrypt_payload(data);
        if (!write_binary(job.full_path, encrypted_data)) {
            write_binary(task_path, "status=failed\nreason=encrypt_write_failed\n");
            continue;
        }

        std::ostringstream meta;
        meta << "checksum=" << s.checksum << "\n";
        meta << "process_us=" << s.process_us << "\n";
        meta << "backend=" << processor_.backendName() << "\n";
        meta << "threads=" << processor_.workers() << "\n";
        meta << "encrypted=1\n";
        meta << "job_id=" << job.id << "\n";
        write_binary(job.full_path + ".meta", meta.str());

        std::ostringstream done;
        done << "status=done\n";
        done << "job_id=" << job.id << "\n";
        done << "file=" << job.file_name << "\n";
        done << "checksum=" << s.checksum << "\n";
        done << "process_us=" << s.process_us << "\n";
        write_binary(task_path, done.str());
    }
}

// Rejects unsafe names containing traversal or separators.
bool Server::safe_file_name(const std::string& name) {
    if (name.empty()) {
        return false;
    }
    if (name.find("..") != std::string::npos) {
        return false;
    }
    if (name.find('/') != std::string::npos || name.find('\\') != std::string::npos) {
        return false;
    }
    return true;
}

// Extracts the first query parameter value for a key.
std::string Server::get_query_value(const std::string& query, const std::string& key) {
    const std::string token = key + "=";
    std::string::size_type start = query.find(token);
    if (start == std::string::npos) {
        return "";
    }
    std::string::size_type end = query.find('&', start);
    if (end == std::string::npos) {
        end = query.size();
    }
    return query.substr(start + token.size(), end - (start + token.size()));
}

// Reads an entire file into a byte string.
bool Server::read_binary(const std::string& path, std::string& out) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        return false;
    }
    out.clear();
    char buf[4096];
    while (1) {
        size_t n = fread(buf, 1, sizeof(buf), f);
        if (n > 0) {
            out.append(buf, n);
        }
        if (n < sizeof(buf)) {
            break;
        }
    }
    fclose(f);
    return true;
}

// Writes a byte string to a file path.
bool Server::write_binary(const std::string& path, const std::string& data) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) {
        return false;
    }
    if (!data.empty()) {
        fwrite(data.data(), 1, data.size(), f);
    }
    fclose(f);
    return true;
}

// Builds a minimal HTTP response text block.
std::string Server::build_http(const std::string& body, const std::string& status, const std::string& content_type) {
    std::ostringstream out;
    out << "HTTP/1.1 " << status << "\r\n";
    out << "Content-Type: " << content_type << "\r\n";
    out << "Content-Length: " << body.size() << "\r\n";
    out << "Connection: close\r\n\r\n";
    out << body;
    return out.str();
}
