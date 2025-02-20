#include "web_app.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>

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

bool writeBinaryFile(const std::string& filePath, const std::string& data) {
    std::ofstream out(filePath, std::ios::binary);
    if (!out.is_open()) {
        return false;
    }
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
    return out.good();
}

std::pair<std::string, std::string> splitPathAndQuery(const std::string& target) {
    const std::size_t q = target.find('?');
    if (q == std::string::npos) {
        return {target, ""};
    }
    return {target.substr(0, q), target.substr(q + 1)};
}
} // namespace

WebApp::WebApp(const std::string& dbPath,
                             const std::string& staticRoot,
                             int processRounds,
                             std::size_t workerThreads,
                             ComputeBackend backend)
    : repo_(dbPath),
            backgroundProcessor_(processRounds, workerThreads, backend),
            backgroundPool_(1),
      staticRoot_(staticRoot),
      uploadRoot_(staticRoot + "/uploads") {
    std::filesystem::create_directories(uploadRoot_);
}

HttpResponse WebApp::handle(const HttpRequest& request) {
    const auto [pathOnly, _query] = splitPathAndQuery(request.path);

    HttpRequest req = request;
    req.path = pathOnly;

    if ((req.path == "/register" || req.path == "/login") && req.method == "POST") {
        return handleAuth(req);
    }

    if (req.path == "/upload" && (req.method == "POST" || req.method == "GET")) {
        req.path = request.path; // 保留 query 供上传参数解析
        return handleUpload(req);
    }

    if (req.path.rfind("/files/", 0) == 0 && req.method == "GET") {
        return handleDownload(req);
    }

    if (req.path.rfind("/media/", 0) == 0 && req.method == "GET") {
        return handleMedia(req);
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

HttpResponse WebApp::handleUpload(const HttpRequest& request) {
    const auto [pathOnly, query] = splitPathAndQuery(request.path);
    (void)pathOnly;

    const std::string fileName = getQueryValue(query, "name");
    if (!isSafeFileName(fileName)) {
        return {"400 Bad Request", "text/plain; charset=utf-8", "invalid file name\n"};
    }

    std::string content = request.body;
    if (content.empty()) {
        // Webbench 场景下通常只能发 GET，这里允许 query 中的 content 作为上传内容。
        content = getQueryValue(query, "content");
    }

    const std::string fullPath = uploadRoot_ + "/" + fileName;
    if (!writeBinaryFile(fullPath, content)) {
        return {"500 Internal Server Error", "text/plain; charset=utf-8", "upload failed\n"};
    }

    const std::uint64_t jobId = nextJobId_.fetch_add(1);
    enqueueBackgroundProcess(fullPath, fileName, jobId);

    std::ostringstream resp;
    resp << "upload success: /files/" << fileName << "\n";
    resp << "mode=online-noomp background-" << backgroundProcessor_.backendName() << " job_id=" << jobId << "\n";
    resp << "task status file: /files/" << fileName << ".task\n";
    return {"200 OK", "text/plain; charset=utf-8", resp.str()};
}

HttpResponse WebApp::handleDownload(const HttpRequest& request) {
    const std::string fileName = request.path.substr(std::string("/files/").size());
    if (!isSafeFileName(fileName)) {
        return {"400 Bad Request", "text/plain; charset=utf-8", "invalid file name\n"};
    }

    const std::string fullPath = uploadRoot_ + "/" + fileName;
    std::string fileData;
    if (!readBinaryFile(fullPath, fileData)) {
        return {"404 Not Found", "text/plain; charset=utf-8", "file not found\n"};
    }

    // 在线下载路径保持 noomp，仅做文件读取与回包。
    return {"200 OK", mimeFromPath(fullPath), std::move(fileData)};
}

void WebApp::enqueueBackgroundProcess(const std::string& fullPath,
                                      const std::string& fileName,
                                      std::uint64_t jobId) {
    const std::string taskPath = fullPath + ".task";
    const std::string queued = "status=queued\njob_id=" + std::to_string(jobId) +
                               "\nbackend=" + backgroundProcessor_.backendName() + "\n";
    (void)writeBinaryFile(taskPath, queued);

    backgroundPool_.enqueue([this, fullPath, fileName, taskPath, jobId]() {
        (void)writeBinaryFile(taskPath,
                              "status=running\njob_id=" + std::to_string(jobId) +
                                  "\nbackend=" + std::string(backgroundProcessor_.backendName()) + "\n");

        std::string fileData;
        if (!readBinaryFile(fullPath, fileData)) {
            (void)writeBinaryFile(taskPath,
                                  "status=failed\njob_id=" + std::to_string(jobId) + "\nreason=read_failed\n");
            return;
        }

        const ProcessingStats stats = backgroundProcessor_.process(fileData);

        std::ostringstream meta;
        meta << "checksum=" << stats.checksum << "\n";
        meta << "process_us=" << stats.processMicros << "\n";
        meta << "backend=" << backgroundProcessor_.backendName() << "\n";
        meta << "threads=" << backgroundProcessor_.workerThreads() << "\n";
        meta << "job_id=" << jobId << "\n";
        (void)writeBinaryFile(fullPath + ".meta", meta.str());

        std::ostringstream done;
        done << "status=done\n";
        done << "job_id=" << jobId << "\n";
        done << "file=" << fileName << "\n";
        done << "checksum=" << stats.checksum << "\n";
        done << "process_us=" << stats.processMicros << "\n";
        (void)writeBinaryFile(taskPath, done.str());
    });
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

std::string WebApp::getQueryValue(const std::string& query, const std::string& key) {
    const std::string token = key + "=";
    const std::size_t start = query.find(token);
    if (start == std::string::npos) {
        return "";
    }

    std::size_t end = query.find('&', start);
    if (end == std::string::npos) {
        end = query.size();
    }

    return query.substr(start + token.size(), end - (start + token.size()));
}

bool WebApp::isSafeFileName(const std::string& fileName) {
    if (fileName.empty()) {
        return false;
    }
    if (fileName.find("..") != std::string::npos) {
        return false;
    }
    if (fileName.find('/') != std::string::npos || fileName.find('\\') != std::string::npos) {
        return false;
    }
    return true;
}
