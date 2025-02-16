#pragma once

#include "data_processor.h"
#include "http_parser.h"
#include "user_repository.h"

#include <string>

struct HttpResponse {
    std::string status;
    std::string contentType;
    std::string body;
};

class WebApp {
public:
    WebApp(const std::string& dbPath, const std::string& staticRoot, bool useOpenMP, int processRounds);

    HttpResponse handle(const HttpRequest& request);

private:
    HttpResponse handleAuth(const HttpRequest& request);
    HttpResponse handleMedia(const HttpRequest& request);
    HttpResponse handleUpload(const HttpRequest& request);
    HttpResponse handleDownload(const HttpRequest& request);

    static std::string getFormValue(const std::string& body, const std::string& key);
    static std::string getQueryValue(const std::string& query, const std::string& key);
    static bool isSafeFileName(const std::string& fileName);

    UserRepository repo_;
    DataProcessor processor_;
    std::string staticRoot_;
    std::string uploadRoot_;
};
