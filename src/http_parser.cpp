#include "http_parser.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace {
std::string trim(std::string value) {
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

bool parseContentLength(const std::unordered_map<std::string, std::string>& headers, std::size_t& bodyLen) {
    auto it = headers.find("Content-Length");
    if (it == headers.end()) {
        bodyLen = 0;
        return true;
    }

    try {
        bodyLen = static_cast<std::size_t>(std::stoull(it->second));
        return true;
    } catch (...) {
        return false;
    }
}
} // namespace

ParseResult HttpParser::parse(const std::string& data, HttpRequest& request, std::size_t& consumedBytes) const {
    consumedBytes = 0;

    const std::size_t headerEnd = data.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        return ParseResult::Incomplete;
    }

    std::istringstream stream(data.substr(0, headerEnd));
    std::string line;

    if (!std::getline(stream, line)) {
        return ParseResult::Error;
    }
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }

    {
        std::istringstream reqLine(line);
        if (!(reqLine >> request.method >> request.path >> request.version)) {
            return ParseResult::Error;
        }
    }

    request.headers.clear();
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            break;
        }

        const std::size_t sep = line.find(':');
        if (sep == std::string::npos) {
            return ParseResult::Error;
        }

        std::string key = trim(line.substr(0, sep));
        std::string value = trim(line.substr(sep + 1));
        request.headers[key] = value;
    }

    std::size_t bodyLen = 0;
    if (!parseContentLength(request.headers, bodyLen)) {
        return ParseResult::Error;
    }

    const std::size_t total = headerEnd + 4 + bodyLen;
    if (data.size() < total) {
        return ParseResult::Incomplete;
    }

    request.body = data.substr(headerEnd + 4, bodyLen);

    if (request.method != "GET" && request.method != "POST") {
        return ParseResult::Error;
    }

    consumedBytes = total;
    return ParseResult::Complete;
}
