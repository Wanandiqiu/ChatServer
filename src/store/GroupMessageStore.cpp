#include "store/GroupMessageStore.hpp"

#include "ErrCode.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>

namespace {

sqlite3* asDb(void* ptr) {
    return static_cast<sqlite3*>(ptr);
}

int64_t nowUnix() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

}  // namespace

bool GroupMessageStore::init(const std::string& db_path) {
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
        std::cerr << "GroupMessageStore sqlite3_open failed\n";
        if (raw) {
            sqlite3_close(raw);
        }
        return false;
    }

    db_ = raw;

    const char* schema = R"(
        CREATE TABLE IF NOT EXISTS group_message (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            group_id INTEGER NOT NULL,
            from_uid INTEGER NOT NULL,
            content TEXT NOT NULL,
            sent_at INTEGER NOT NULL
        );
        CREATE TABLE IF NOT EXISTS group_message_delivery (
            msg_id INTEGER NOT NULL,
            user_id INTEGER NOT NULL,
            delivered INTEGER NOT NULL DEFAULT 0,
            PRIMARY KEY (msg_id, user_id)
        );
    )";

    char* err = nullptr;
    if (sqlite3_exec(asDb(db_), schema, nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << "GroupMessageStore schema failed: " << (err ? err : "unknown") << '\n';
        sqlite3_free(err);
        return false;
    }

    return true;
}

SaveGroupMessageResult GroupMessageStore::saveMessage(int from_uid,
                                                      int group_id,
                                                      const std::string& content) {
    SaveGroupMessageResult result;

    if (content.empty()) {
        result.errcode = errc::kInvalidRequest;
        result.errmsg = "empty message";
        return result;
    }

    const char* sql =
        "INSERT INTO group_message (group_id, from_uid, content, sent_at) VALUES (?, ?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(asDb(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        result.errcode = errc::kInternalError;
        result.errmsg = sqlite3_errmsg(asDb(db_));
        return result;
    }

    const int64_t sent_at = nowUnix();
    sqlite3_bind_int(stmt, 1, group_id);
    sqlite3_bind_int(stmt, 2, from_uid);
    sqlite3_bind_text(stmt, 3, content.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, sent_at);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        result.errcode = errc::kInternalError;
        result.errmsg = sqlite3_errmsg(asDb(db_));
        sqlite3_finalize(stmt);
        return result;
    }

    sqlite3_finalize(stmt);

    result.errcode = errc::kOk;
    result.msg_id = sqlite3_last_insert_rowid(asDb(db_));
    result.sent_at = sent_at;
    result.errmsg = "saved";
    return result;
}

bool GroupMessageStore::insertDelivery(int64_t msg_id, int user_id, int delivered) {
    const char* sql =
        "INSERT INTO group_message_delivery (msg_id, user_id, delivered) VALUES (?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(asDb(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, msg_id);
    sqlite3_bind_int(stmt, 2, user_id);
    sqlite3_bind_int(stmt, 3, delivered);

    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

bool GroupMessageStore::markDelivered(int64_t msg_id, int user_id) {
    const char* sql =
        "UPDATE group_message_delivery SET delivered = 1 WHERE msg_id = ? AND user_id = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(asDb(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, msg_id);
    sqlite3_bind_int(stmt, 2, user_id);

    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

std::vector<GroupMessage> GroupMessageStore::fetchUndeliveredForUser(int user_id) {
    const char* sql =
        "SELECT m.id, m.group_id, m.from_uid, m.content, m.sent_at "
        "FROM group_message m "
        "INNER JOIN group_message_delivery d ON m.id = d.msg_id "
        "WHERE d.user_id = ? AND d.delivered = 0 "
        "ORDER BY m.id ASC;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(asDb(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return {};
    }

    sqlite3_bind_int(stmt, 1, user_id);

    std::vector<GroupMessage> messages;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        GroupMessage msg;
        msg.id = sqlite3_column_int64(stmt, 0);
        msg.group_id = sqlite3_column_int(stmt, 1);
        msg.from_uid = sqlite3_column_int(stmt, 2);
        msg.content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        msg.sent_at = sqlite3_column_int64(stmt, 4);
        messages.push_back(std::move(msg));
    }

    sqlite3_finalize(stmt);
    return messages;
}

std::vector<GroupMessage> GroupMessageStore::fetchGroupHistory(int group_id,
                                                               int limit,
                                                               int64_t before_msg_id) {
    const char* sql =
        "SELECT id, group_id, from_uid, content, sent_at FROM group_message "
        "WHERE group_id = ? AND (? = 0 OR id < ?) "
        "ORDER BY id DESC LIMIT ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(asDb(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return {};
    }

    sqlite3_bind_int(stmt, 1, group_id);
    sqlite3_bind_int64(stmt, 2, before_msg_id);
    sqlite3_bind_int64(stmt, 3, before_msg_id);
    sqlite3_bind_int(stmt, 4, limit);

    std::vector<GroupMessage> messages;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        GroupMessage msg;
        msg.id = sqlite3_column_int64(stmt, 0);
        msg.group_id = sqlite3_column_int(stmt, 1);
        msg.from_uid = sqlite3_column_int(stmt, 2);
        msg.content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        msg.sent_at = sqlite3_column_int64(stmt, 4);
        messages.push_back(std::move(msg));
    }

    sqlite3_finalize(stmt);
    std::reverse(messages.begin(), messages.end());
    return messages;
}
