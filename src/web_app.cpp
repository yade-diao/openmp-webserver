#include "web_app.h"

#include <fstream>
#include <sstream>

namespace {
std::string mimeFromPath(const std::string& path) {
    const auto pos = path.find_last_of('.');
    if (pos == std::string::npos) {
        return "application/octet-stream";
    }

    const std::string ext = path.substr(pos + 1);
    if (ext == "jpg" || ext == "jpeg") {
        return "image/jpeg";
    }
    if (ext == "png") {
        return "image/png";
    }
    if (ext == "gif") {
        return "image/gif";
    }
    if (ext == "mp4") {
        return "video/mp4";
    }
    if (ext == "webm") {
        return "video/webm";
    }

    return "application/octet-stream";
}

bool readBinaryFile(const std::string& filePath, std::string& out) {
    std::ifstream in(filePath, std::ios::binary);
    if (!in.is_open()) {
        return false;
    }

    std::ostringstream buffer;
    buffer << in.rdbuf();
    out = buffer.str();
    return true;
}
} // namespace

WebApp::WebApp(const std::string& dbPath, const std::string& staticRoot)
    : repo_(dbPath), staticRoot_(staticRoot) {}

HttpResponse WebApp::handle(const HttpRequest& request) {
    if ((request.path == "/register" || request.path == "/login") && request.method == "POST") {
        return handleAuth(request);
    }

    if (request.path.rfind("/media/", 0) == 0 && request.method == "GET") {
        return handleMedia(request);
    }

    return {"404 Not Found", "text/plain; charset=utf-8", "Not Found\n"};
}

HttpResponse WebApp::handleAuth(const HttpRequest& request) {
    const std::string username = getFormValue(request.body, "username");
    const std::string password = getFormValue(request.body, "password");

    if (username.empty() || password.empty()) {
        return {"400 Bad Request", "text/plain; charset=utf-8", "username/password required\n"};
    }

    AuthResult result;
    if (request.path == "/register") {
        result = repo_.registerUser(username, password);
    } else {
        result = repo_.loginUser(username, password);
    }

    if (result.ok) {
        return {"200 OK", "text/plain; charset=utf-8", result.message + "\n"};
    }

    return {"401 Unauthorized", "text/plain; charset=utf-8", result.message + "\n"};
}

HttpResponse WebApp::handleMedia(const HttpRequest& request) {
    const std::string fileName = request.path.substr(std::string("/media/").size());
    if (fileName.empty() || fileName.find("..") != std::string::npos) {
        return {"400 Bad Request", "text/plain; charset=utf-8", "invalid media path\n"};
    }

    const std::string fullPath = staticRoot_ + "/" + fileName;
    std::string fileData;
    if (!readBinaryFile(fullPath, fileData)) {
        return {"404 Not Found", "text/plain; charset=utf-8", "media not found\n"};
    }

    return {"200 OK", mimeFromPath(fullPath), std::move(fileData)};
}

std::string WebApp::getFormValue(const std::string& body, const std::string& key) {
    const std::string token = key + "=";
    const std::size_t start = body.find(token);
    if (start == std::string::npos) {
        return "";
    }

    std::size_t end = body.find('&', start);
    if (end == std::string::npos) {
        end = body.size();
    }

    return body.substr(start + token.size(), end - (start + token.size()));
}
