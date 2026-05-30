#include "store/FriendStore.hpp"

#include "ErrCode.hpp"

#include <sqlite3.h>

#include <filesystem>
#include <iostream>

namespace {

sqlite3* asDb(void* ptr) {
    return static_cast<sqlite3*>(ptr);
}

}  // namespace

bool FriendStore::init(const std::string& db_path) {
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
        std::cerr << "FriendStore sqlite3_open failed\n";
        if (raw) {
            sqlite3_close(raw);
        }
        return false;
    }

    db_ = raw;

    const char* schema = R"(
        CREATE TABLE IF NOT EXISTS friend (
            user_id INTEGER NOT NULL,
            friend_id INTEGER NOT NULL,
            PRIMARY KEY (user_id, friend_id)
        );
    )";

    char* err = nullptr;
    if (sqlite3_exec(asDb(db_), schema, nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << "FriendStore schema failed: " << (err ? err : "unknown") << '\n';
        sqlite3_free(err);
        return false;
    }

    return true;
}

FriendOpResult FriendStore::addFriend(int uid, int friend_uid) {
    FriendOpResult result;

    if (uid == friend_uid) {
        result.errcode = errc::kCannotAddSelf;
        result.errmsg = "cannot add yourself";
        return result;
    }

    if (areFriends(uid, friend_uid)) {
        result.errcode = errc::kAlreadyFriend;
        result.errmsg = "already friends";
        result.friend_uid = friend_uid;
        return result;
    }

    const char* sql = "INSERT INTO friend (user_id, friend_id) VALUES (?, ?);";
    sqlite3_stmt* stmt = nullptr;

    auto insert_one = [&](int a, int b) -> bool {
        if (sqlite3_prepare_v2(asDb(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return false;
        }
        sqlite3_bind_int(stmt, 1, a);
        sqlite3_bind_int(stmt, 2, b);
        const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);
        stmt = nullptr;
        return ok;
    };

    if (!insert_one(uid, friend_uid) || !insert_one(friend_uid, uid)) {
        result.errcode = errc::kInternalError;
        result.errmsg = sqlite3_errmsg(asDb(db_));
        return result;
    }

    result.errcode = errc::kOk;
    result.friend_uid = friend_uid;
    result.errmsg = "friend added";
    return result;
}

bool FriendStore::areFriends(int uid, int friend_uid) const {
    const char* sql =
        "SELECT 1 FROM friend WHERE user_id = ? AND friend_id = ? LIMIT 1;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(asDb(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt, 1, uid);
    sqlite3_bind_int(stmt, 2, friend_uid);

    const bool found = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    return found;
}
