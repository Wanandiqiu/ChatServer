#include "store/MessageStore.hpp"

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

bool MessageStore::init(const std::string& db_path) {
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
        std::cerr << "MessageStore sqlite3_open failed\n";
        if (raw) {
            sqlite3_close(raw);
        }
        return false;
    }

    db_ = raw;

    const char* schema = R"(
        CREATE TABLE IF NOT EXISTS chat_message (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            from_id INTEGER NOT NULL,
            to_id INTEGER NOT NULL,
            content TEXT NOT NULL,
            sent_at INTEGER NOT NULL,
            delivered INTEGER NOT NULL DEFAULT 0
        );
    )";

    char* err = nullptr;
    if (sqlite3_exec(asDb(db_), schema, nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << "MessageStore schema failed: " << (err ? err : "unknown") << '\n';
        sqlite3_free(err);
        return false;
    }

    return true;
}

SaveMessageResult MessageStore::saveMessage(int from_id, int to_id, const std::string& content) {
    SaveMessageResult result;

    if (content.empty()) {
        result.errcode = errc::kInvalidRequest;
        result.errmsg = "empty message";
        return result;
    }

    const char* sql =
        "INSERT INTO chat_message (from_id, to_id, content, sent_at, delivered) "
        "VALUES (?, ?, ?, ?, 0);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(asDb(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        result.errcode = errc::kInternalError;
        result.errmsg = sqlite3_errmsg(asDb(db_));
        return result;
    }

    const int64_t sent_at = nowUnix();
    sqlite3_bind_int(stmt, 1, from_id);
    sqlite3_bind_int(stmt, 2, to_id);
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

std::vector<ChatMessage> MessageStore::fetchUndelivered(int to_id) {
    const char* sql =
        "SELECT id, from_id, to_id, content, sent_at FROM chat_message "
        "WHERE to_id = ? AND delivered = 0 ORDER BY id ASC;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(asDb(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return {};
    }

    sqlite3_bind_int(stmt, 1, to_id);

    std::vector<ChatMessage> messages;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ChatMessage msg;
        msg.id = sqlite3_column_int64(stmt, 0);
        msg.from_id = sqlite3_column_int(stmt, 1);
        msg.to_id = sqlite3_column_int(stmt, 2);
        msg.content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        msg.sent_at = sqlite3_column_int64(stmt, 4);
        messages.push_back(std::move(msg));
    }

    sqlite3_finalize(stmt);
    return messages;
}

std::vector<ChatMessage> MessageStore::fetchHistoryBetween(int self_uid,
                                                           int peer_uid,
                                                           int limit,
                                                           int64_t before_msg_id) {
    const char* sql =
        "SELECT id, from_id, to_id, content, sent_at FROM chat_message "
        "WHERE ((from_id = ? AND to_id = ?) OR (from_id = ? AND to_id = ?)) "
        "AND (? = 0 OR id < ?) "
        "ORDER BY id DESC LIMIT ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(asDb(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return {};
    }

    sqlite3_bind_int(stmt, 1, self_uid);
    sqlite3_bind_int(stmt, 2, peer_uid);
    sqlite3_bind_int(stmt, 3, peer_uid);
    sqlite3_bind_int(stmt, 4, self_uid);
    sqlite3_bind_int64(stmt, 5, before_msg_id);
    sqlite3_bind_int64(stmt, 6, before_msg_id);
    sqlite3_bind_int(stmt, 7, limit);

    std::vector<ChatMessage> messages;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ChatMessage msg;
        msg.id = sqlite3_column_int64(stmt, 0);
        msg.from_id = sqlite3_column_int(stmt, 1);
        msg.to_id = sqlite3_column_int(stmt, 2);
        msg.content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        msg.sent_at = sqlite3_column_int64(stmt, 4);
        messages.push_back(std::move(msg));
    }

    sqlite3_finalize(stmt);
    std::reverse(messages.begin(), messages.end());
    return messages;
}

bool MessageStore::markDelivered(int64_t msg_id) {
    const char* sql = "UPDATE chat_message SET delivered = 1 WHERE id = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(asDb(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, msg_id);
    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}
