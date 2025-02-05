#pragma once

#include <mutex>
#include <string>

struct AuthResult {
    bool ok{false};
    std::string message;
};

class UserRepository {
public:
    explicit UserRepository(const std::string& dbPath);
    ~UserRepository();

    UserRepository(const UserRepository&) = delete;
    UserRepository& operator=(const UserRepository&) = delete;

    AuthResult registerUser(const std::string& username, const std::string& password);
    AuthResult loginUser(const std::string& username, const std::string& password);

private:
    void initSchema();

    void* db_{nullptr};
    std::mutex mutex_;
};
