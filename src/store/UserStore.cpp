#include "store/UserStore.hpp"

#include "ErrCode.hpp"
#include "auth/PasswordHash.hpp"

#include <sqlite3.h>

#include <filesystem>
#include <iostream>
#include <utility>

namespace {

sqlite3* asDb(void* ptr) {
    return static_cast<sqlite3*>(ptr);
}

}  // namespace

bool UserStore::init(const std::string& db_path) {
    if (db_) {
        sqlite3_close(asDb(db_));
        db_ = nullptr;
    }

    const auto parent = std::filesystem::path(db_path).parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
    }

    sqlite3* raw = nullptr;
    if (sqlite3_open(db_path.c_str(), &raw) != SQLITE_OK) {
        std::cerr << "sqlite3_open failed: " << sqlite3_errmsg(raw) << '\n';
        if (raw) {
            sqlite3_close(raw);
        }
        return false;
    }

    db_ = raw;

    const char* schema = R"(
        CREATE TABLE IF NOT EXISTS user (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT UNIQUE NOT NULL,
            password_hash TEXT NOT NULL,
            state TEXT NOT NULL DEFAULT 'offline'
        );
    )";

    char* err = nullptr;
    if (sqlite3_exec(asDb(db_), schema, nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << "schema init failed: " << (err ? err : "unknown") << '\n';
        sqlite3_free(err);
        return false;
    }

    return true;
}

CreateUserResult UserStore::createUser(const std::string& name,
                                       const std::string& plain_password) {
    CreateUserResult result;

    if (name.empty() || plain_password.empty()) {
        result.errcode = errc::kInvalidRequest;
        result.errmsg = "name and password required";
        return result;
    }

    if (findByName(name)) {
        result.errcode = errc::kUserExists;
        result.errmsg = "username already exists";
        return result;
    }

    std::string encoded;
    try {
        encoded = auth::hashPassword(plain_password);
    } catch (const std::exception& ex) {
        result.errcode = errc::kInternalError;
        result.errmsg = ex.what();
        return result;
    }

    const char* sql =
        "INSERT INTO user (name, password_hash, state) VALUES (?, ?, 'offline');";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(asDb(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        result.errcode = errc::kInternalError;
        result.errmsg = sqlite3_errmsg(asDb(db_));
        return result;
    }

    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, encoded.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        result.errcode = errc::kInternalError;
        result.errmsg = sqlite3_errmsg(asDb(db_));
        sqlite3_finalize(stmt);
        return result;
    }

    sqlite3_finalize(stmt);

    result.errcode = errc::kOk;
    result.uid = static_cast<int>(sqlite3_last_insert_rowid(asDb(db_)));
    result.errmsg = "register ok";
    return result;
}

std::optional<UserRecord> UserStore::findByName(const std::string& name) {
    const char* sql = "SELECT id, name, password_hash, state FROM user WHERE name = ? LIMIT 1;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(asDb(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);

    std::optional<UserRecord> record;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        UserRecord user;
        user.uid = sqlite3_column_int(stmt, 0);
        user.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        user.password_hash =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        user.state = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        record = std::move(user);
    }

    sqlite3_finalize(stmt);
    return record;
}

std::optional<UserRecord> UserStore::findByUid(int uid) {
    const char* sql = "SELECT id, name, password_hash, state FROM user WHERE id = ? LIMIT 1;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(asDb(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_int(stmt, 1, uid);

    std::optional<UserRecord> record;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        UserRecord user;
        user.uid = sqlite3_column_int(stmt, 0);
        user.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        user.password_hash =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        user.state = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        record = std::move(user);
    }

    sqlite3_finalize(stmt);
    return record;
}

bool UserStore::uidExists(int uid) {
    return findByUid(uid).has_value();
}

UpdatePasswordResult UserStore::updatePassword(const std::string& name,
                                             const std::string& old_plain_password,
                                             const std::string& new_plain_password) {
    UpdatePasswordResult result;

    if (name.empty() || old_plain_password.empty() || new_plain_password.empty()) {
        result.errcode = errc::kInvalidRequest;
        result.errmsg = "username, old and new password required";
        return result;
    }

    const auto user = findByName(name);
    if (!user) {
        result.errcode = errc::kUserNotFound;
        result.errmsg = "user not found";
        return result;
    }

    if (!auth::verifyPassword(old_plain_password, user->password_hash)) {
        result.errcode = errc::kWrongPassword;
        result.errmsg = "old password incorrect";
        return result;
    }

    std::string encoded;
    try {
        encoded = auth::hashPassword(new_plain_password);
    } catch (const std::exception& ex) {
        result.errcode = errc::kInternalError;
        result.errmsg = ex.what();
        return result;
    }

    const char* sql = "UPDATE user SET password_hash = ? WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(asDb(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        result.errcode = errc::kInternalError;
        result.errmsg = sqlite3_errmsg(asDb(db_));
        return result;
    }

    sqlite3_bind_text(stmt, 1, encoded.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, user->uid);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        result.errcode = errc::kInternalError;
        result.errmsg = sqlite3_errmsg(asDb(db_));
        sqlite3_finalize(stmt);
        return result;
    }

    sqlite3_finalize(stmt);
    result.errcode = errc::kOk;
    result.errmsg = "password updated";
    return result;
}
