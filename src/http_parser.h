#pragma once

#include <string>
#include <unordered_map>

struct HttpRequest {
    std::string method;
    std::string path;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
};

enum class ParseResult {
    Complete,
    Incomplete,
    Error,
};

class HttpParser {
public:
    ParseResult parse(const std::string& data, HttpRequest& request, std::size_t& consumedBytes) const;
};
