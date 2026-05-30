#include "store/ConversationStore.hpp"

#include <sqlite3.h>

#include <filesystem>
#include <iostream>

namespace {

sqlite3* asDb(void* ptr) {
    return static_cast<sqlite3*>(ptr);
}

int sessionTypeToInt(ConversationSessionType type) {
    return static_cast<int>(type);
}

ConversationSessionType intToSessionType(int value) {
    if (value == static_cast<int>(ConversationSessionType::Group)) {
        return ConversationSessionType::Group;
    }
    return ConversationSessionType::Peer;
}

}  // namespace

bool ConversationStore::init(const std::string& db_path) {
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
        std::cerr << "ConversationStore sqlite3_open failed\n";
        if (raw) {
            sqlite3_close(raw);
        }
        return false;
    }

    db_ = raw;

    const char* schema = R"(
        CREATE TABLE IF NOT EXISTS conversation_state (
            user_id INTEGER NOT NULL,
            session_type INTEGER NOT NULL,
            target_id INTEGER NOT NULL,
            title TEXT NOT NULL DEFAULT '',
            last_msg_id INTEGER NOT NULL DEFAULT 0,
            last_msg_at INTEGER NOT NULL DEFAULT 0,
            last_preview TEXT NOT NULL DEFAULT '',
            last_read_msg_id INTEGER NOT NULL DEFAULT 0,
            unread_count INTEGER NOT NULL DEFAULT 0,
            PRIMARY KEY (user_id, session_type, target_id)
        );
    )";

    char* err = nullptr;
    if (sqlite3_exec(asDb(db_), schema, nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << "ConversationStore schema failed: " << (err ? err : "unknown") << '\n';
        sqlite3_free(err);
        return false;
    }

    return true;
}

void ConversationStore::touchOutgoing(int user_id,
                                      ConversationSessionType session_type,
                                      int32_t target_id,
                                      const std::string& title,
                                      const std::string& preview,
                                      int64_t msg_id,
                                      int64_t sent_at) {
    if (!db_ || msg_id <= 0) {
        return;
    }

    const char* sql = R"(
        INSERT INTO conversation_state (
            user_id, session_type, target_id, title,
            last_msg_id, last_msg_at, last_preview,
            last_read_msg_id, unread_count
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, 0)
        ON CONFLICT(user_id, session_type, target_id) DO UPDATE SET
            title = excluded.title,
            last_msg_id = excluded.last_msg_id,
            last_msg_at = excluded.last_msg_at,
            last_preview = excluded.last_preview
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(asDb(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return;
    }

    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_bind_int(stmt, 2, sessionTypeToInt(session_type));
    sqlite3_bind_int(stmt, 3, target_id);
    sqlite3_bind_text(stmt, 4, title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 5, msg_id);
    sqlite3_bind_int64(stmt, 6, sent_at);
    sqlite3_bind_text(stmt, 7, preview.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 8, msg_id);

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void ConversationStore::touchIncoming(int user_id,
                                      ConversationSessionType session_type,
                                      int32_t target_id,
                                      const std::string& title,
                                      const std::string& preview,
                                      int64_t msg_id,
                                      int64_t sent_at) {
    if (!db_ || msg_id <= 0) {
        return;
    }

    const char* sql = R"(
        INSERT INTO conversation_state (
            user_id, session_type, target_id, title,
            last_msg_id, last_msg_at, last_preview,
            last_read_msg_id, unread_count
        ) VALUES (?, ?, ?, ?, ?, ?, ?, 0, 1)
        ON CONFLICT(user_id, session_type, target_id) DO UPDATE SET
            title = excluded.title,
            last_msg_id = excluded.last_msg_id,
            last_msg_at = excluded.last_msg_at,
            last_preview = excluded.last_preview,
            unread_count = unread_count + 1
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(asDb(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return;
    }

    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_bind_int(stmt, 2, sessionTypeToInt(session_type));
    sqlite3_bind_int(stmt, 3, target_id);
    sqlite3_bind_text(stmt, 4, title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 5, msg_id);
    sqlite3_bind_int64(stmt, 6, sent_at);
    sqlite3_bind_text(stmt, 7, preview.c_str(), -1, SQLITE_TRANSIENT);

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

std::vector<ConversationRow> ConversationStore::listConversations(int user_id) const {
    std::vector<ConversationRow> rows;
    if (!db_) {
        return rows;
    }

    const char* sql = R"(
        SELECT session_type, target_id, title, last_msg_id, last_msg_at,
               last_preview, last_read_msg_id, unread_count
        FROM conversation_state
        WHERE user_id = ? AND last_msg_id > 0
        ORDER BY last_msg_at DESC
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(asDb(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return rows;
    }

    sqlite3_bind_int(stmt, 1, user_id);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ConversationRow row;
        row.session_type = intToSessionType(sqlite3_column_int(stmt, 0));
        row.target_id = sqlite3_column_int(stmt, 1);
        if (const auto* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2))) {
            row.title = text;
        }
        row.last_msg_id = sqlite3_column_int64(stmt, 3);
        row.last_msg_at = sqlite3_column_int64(stmt, 4);
        if (const auto* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5))) {
            row.last_preview = text;
        }
        row.last_read_msg_id = sqlite3_column_int64(stmt, 6);
        row.unread_count = sqlite3_column_int(stmt, 7);
        rows.push_back(row);
    }

    sqlite3_finalize(stmt);
    return rows;
}

bool ConversationStore::markRead(int user_id,
                                 ConversationSessionType session_type,
                                 int32_t target_id,
                                 int64_t read_through_msg_id) {
    if (!db_) {
        return false;
    }

    if (!conversationExists(user_id, session_type, target_id)) {
        return false;
    }

    const char* sql = R"(
        UPDATE conversation_state
        SET unread_count = 0,
            last_read_msg_id = CASE
                WHEN ? > 0 THEN ?
                ELSE last_msg_id
            END
        WHERE user_id = ? AND session_type = ? AND target_id = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(asDb(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, read_through_msg_id);
    sqlite3_bind_int64(stmt, 2, read_through_msg_id);
    sqlite3_bind_int(stmt, 3, user_id);
    sqlite3_bind_int(stmt, 4, sessionTypeToInt(session_type));
    sqlite3_bind_int(stmt, 5, target_id);

    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

bool ConversationStore::conversationExists(int user_id,
                                             ConversationSessionType session_type,
                                             int32_t target_id) const {
    if (!db_) {
        return false;
    }

    const char* sql = R"(
        SELECT 1 FROM conversation_state
        WHERE user_id = ? AND session_type = ? AND target_id = ?
        LIMIT 1
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(asDb(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_bind_int(stmt, 2, sessionTypeToInt(session_type));
    sqlite3_bind_int(stmt, 3, target_id);

    const bool exists = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    return exists;
}
