#pragma once

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
    WebApp(const std::string& dbPath, const std::string& staticRoot);

    HttpResponse handle(const HttpRequest& request);

private:
    HttpResponse handleAuth(const HttpRequest& request);
    HttpResponse handleMedia(const HttpRequest& request);

    static std::string getFormValue(const std::string& body, const std::string& key);

    UserRepository repo_;
    std::string staticRoot_;
};
