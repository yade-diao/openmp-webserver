#pragma once

#include "data_processor.h"
#include "http_parser.h"
#include "thread_pool.h"
#include "user_repository.h"

#include <atomic>
#include <cstdint>
#include <string>

struct HttpResponse {
    std::string status;
    std::string contentType;
    std::string body;
};

class WebApp {
public:
    WebApp(const std::string& dbPath,
           const std::string& staticRoot,
           int processRounds,
           std::size_t workerThreads,
           ComputeBackend backend);

    HttpResponse handle(const HttpRequest& request);

private:
    HttpResponse handleAuth(const HttpRequest& request);
    HttpResponse handleMedia(const HttpRequest& request);
    HttpResponse handleUpload(const HttpRequest& request);
    HttpResponse handleDownload(const HttpRequest& request);
    void enqueueBackgroundProcess(const std::string& fullPath,
                                 const std::string& fileName,
                                 std::uint64_t jobId);

    static std::string getFormValue(const std::string& body, const std::string& key);
    static std::string getQueryValue(const std::string& query, const std::string& key);
    static bool isSafeFileName(const std::string& fileName);

    UserRepository repo_;
    DataProcessor backgroundProcessor_;
    ThreadPool backgroundPool_;
    std::atomic<std::uint64_t> nextJobId_{1};
    std::string staticRoot_;
    std::string uploadRoot_;
};
