#include "user_repository.h"

#include <sqlite3.h>

#include <stdexcept>

UserRepository::UserRepository(const std::string& dbPath) {
    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        throw std::runtime_error("sqlite3_open failed");
    }
    db_ = db;
    initSchema();
}

UserRepository::~UserRepository() {
    if (db_ != nullptr) {
        sqlite3_close(static_cast<sqlite3*>(db_));
        db_ = nullptr;
    }
}

void UserRepository::initSchema() {
    const char* sql =
        "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "username TEXT UNIQUE NOT NULL,"
        "password TEXT NOT NULL"
        ");";

    char* errMsg = nullptr;
    if (sqlite3_exec(static_cast<sqlite3*>(db_), sql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::string err = errMsg ? errMsg : "sqlite3_exec failed";
        sqlite3_free(errMsg);
        throw std::runtime_error(err);
    }
}

AuthResult UserRepository::registerUser(const std::string& username, const std::string& password) {
    std::lock_guard<std::mutex> lock(mutex_);

    static const char* sql = "INSERT INTO users (username, password) VALUES (?, ?);";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return {false, "prepare failed"};
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_TRANSIENT);

    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE) {
        return {true, "register success"};
    }
    if (rc == SQLITE_CONSTRAINT) {
        return {false, "username exists"};
    }
    return {false, "register failed"};
}

AuthResult UserRepository::loginUser(const std::string& username, const std::string& password) {
    std::lock_guard<std::mutex> lock(mutex_);

    static const char* sql = "SELECT 1 FROM users WHERE username = ? AND password = ? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return {false, "prepare failed"};
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_TRANSIENT);

    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_ROW) {
        return {true, "login success"};
    }
    return {false, "invalid username or password"};
}
