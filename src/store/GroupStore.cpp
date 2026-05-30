#include "store/GroupStore.hpp"

#include "ErrCode.hpp"

#include <sqlite3.h>

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

constexpr int kRequestPending = 0;
constexpr int kRequestApproved = 1;
constexpr int kRequestRejected = 2;

}  // namespace

bool GroupStore::init(const std::string& db_path) {
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
        std::cerr << "GroupStore sqlite3_open failed\n";
        if (raw) {
            sqlite3_close(raw);
        }
        return false;
    }

    db_ = raw;

    const char* schema = R"(
        CREATE TABLE IF NOT EXISTS chat_group (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            description TEXT NOT NULL DEFAULT '',
            join_mode INTEGER NOT NULL,
            owner_uid INTEGER NOT NULL,
            created_at INTEGER NOT NULL
        );
        CREATE TABLE IF NOT EXISTS group_member (
            group_id INTEGER NOT NULL,
            user_id INTEGER NOT NULL,
            joined_at INTEGER NOT NULL,
            PRIMARY KEY (group_id, user_id)
        );
        CREATE TABLE IF NOT EXISTS group_join_request (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            group_id INTEGER NOT NULL,
            applicant_uid INTEGER NOT NULL,
            status INTEGER NOT NULL,
            created_at INTEGER NOT NULL
        );
    )";

    char* err = nullptr;
    if (sqlite3_exec(asDb(db_), schema, nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << "GroupStore schema failed: " << (err ? err : "unknown") << '\n';
        sqlite3_free(err);
        return false;
    }

    return true;
}

bool GroupStore::groupExists(int group_id) const {
    const char* sql = "SELECT 1 FROM chat_group WHERE id = ? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(asDb(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_int(stmt, 1, group_id);
    const bool exists = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    return exists;
}

StoreJoinMode GroupStore::getJoinMode(int group_id) const {
    const char* sql = "SELECT join_mode FROM chat_group WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(asDb(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return StoreJoinMode::Public;
    }
    sqlite3_bind_int(stmt, 1, group_id);
    StoreJoinMode mode = StoreJoinMode::Public;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        mode = static_cast<StoreJoinMode>(sqlite3_column_int(stmt, 0));
    }
    sqlite3_finalize(stmt);
    return mode;
}

std::string GroupStore::getGroupName(int group_id) const {
    const char* sql = "SELECT name FROM chat_group WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(asDb(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return {};
    }
    sqlite3_bind_int(stmt, 1, group_id);
    std::string name;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (text) {
            name = text;
        }
    }
    sqlite3_finalize(stmt);
    return name;
}

bool GroupStore::isMember(int uid, int group_id) const {
    const char* sql =
        "SELECT 1 FROM group_member WHERE group_id = ? AND user_id = ? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(asDb(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_int(stmt, 1, group_id);
    sqlite3_bind_int(stmt, 2, uid);
    const bool member = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    return member;
}

bool GroupStore::isOwner(int uid, int group_id) const {
    const char* sql = "SELECT 1 FROM chat_group WHERE id = ? AND owner_uid = ? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(asDb(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_int(stmt, 1, group_id);
    sqlite3_bind_int(stmt, 2, uid);
    const bool owner = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    return owner;
}

int GroupStore::countMembers(int group_id) const {
    const char* sql = "SELECT COUNT(*) FROM group_member WHERE group_id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(asDb(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }
    sqlite3_bind_int(stmt, 1, group_id);
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return count;
}

int GroupStore::countUserGroups(int uid) const {
    const char* sql = "SELECT COUNT(*) FROM group_member WHERE user_id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(asDb(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }
    sqlite3_bind_int(stmt, 1, uid);
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return count;
}

bool GroupStore::hasPendingRequest(int group_id, int applicant_uid) const {
    const char* sql =
        "SELECT 1 FROM group_join_request WHERE group_id = ? AND applicant_uid = ? "
        "AND status = ? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(asDb(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_int(stmt, 1, group_id);
    sqlite3_bind_int(stmt, 2, applicant_uid);
    sqlite3_bind_int(stmt, 3, kRequestPending);
    const bool pending = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    return pending;
}

GroupOpResult GroupStore::addMember(int uid, int group_id) {
    GroupOpResult result;

    if (isMember(uid, group_id)) {
        result.errcode = errc::kAlreadyInGroup;
        result.errmsg = "already in group";
        return result;
    }

    if (countMembers(group_id) >= kMaxGroupMembers) {
        result.errcode = errc::kGroupFull;
        result.errmsg = "group is full";
        return result;
    }

    if (countUserGroups(uid) >= kMaxGroupsPerUser) {
        result.errcode = errc::kUserGroupLimit;
        result.errmsg = "user group limit reached";
        return result;
    }

    const char* sql =
        "INSERT INTO group_member (group_id, user_id, joined_at) VALUES (?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(asDb(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        result.errcode = errc::kInternalError;
        result.errmsg = sqlite3_errmsg(asDb(db_));
        return result;
    }

    sqlite3_bind_int(stmt, 1, group_id);
    sqlite3_bind_int(stmt, 2, uid);
    sqlite3_bind_int64(stmt, 3, nowUnix());

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        result.errcode = errc::kInternalError;
        result.errmsg = sqlite3_errmsg(asDb(db_));
        sqlite3_finalize(stmt);
        return result;
    }

    sqlite3_finalize(stmt);
    result.errcode = errc::kOk;
    result.errmsg = "joined";
    result.joined = true;
    return result;
}

GroupOpResult GroupStore::createGroup(int owner_uid,
                                      const std::string& name,
                                      const std::string& description,
                                      StoreJoinMode join_mode) {
    GroupOpResult result;

    if (name.empty()) {
        result.errcode = errc::kInvalidRequest;
        result.errmsg = "group name required";
        return result;
    }

    if (countUserGroups(owner_uid) >= kMaxGroupsPerUser) {
        result.errcode = errc::kUserGroupLimit;
        result.errmsg = "user group limit reached";
        return result;
    }

    const char* sql =
        "INSERT INTO chat_group (name, description, join_mode, owner_uid, created_at) "
        "VALUES (?, ?, ?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(asDb(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        result.errcode = errc::kInternalError;
        result.errmsg = sqlite3_errmsg(asDb(db_));
        return result;
    }

    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, description.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, static_cast<int>(join_mode));
    sqlite3_bind_int(stmt, 4, owner_uid);
    sqlite3_bind_int64(stmt, 5, nowUnix());

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        result.errcode = errc::kInternalError;
        result.errmsg = sqlite3_errmsg(asDb(db_));
        sqlite3_finalize(stmt);
        return result;
    }

    sqlite3_finalize(stmt);

    const int group_id = static_cast<int>(sqlite3_last_insert_rowid(asDb(db_)));
    const auto added = addMember(owner_uid, group_id);
    if (added.errcode != errc::kOk) {
        return added;
    }

    result.errcode = errc::kOk;
    result.errmsg = "group created";
    result.group_id = group_id;
    return result;
}

GroupOpResult GroupStore::joinPublic(int uid, int group_id) {
    GroupOpResult result;

    if (!groupExists(group_id)) {
        result.errcode = errc::kGroupNotFound;
        result.errmsg = "group not found";
        return result;
    }

    if (getJoinMode(group_id) != StoreJoinMode::Public) {
        result.errcode = errc::kInvalidRequest;
        result.errmsg = "group requires approval";
        return result;
    }

    return addMember(uid, group_id);
}

GroupOpResult GroupStore::requestJoin(int uid, int group_id) {
    GroupOpResult result;

    if (!groupExists(group_id)) {
        result.errcode = errc::kGroupNotFound;
        result.errmsg = "group not found";
        return result;
    }

    if (getJoinMode(group_id) != StoreJoinMode::ApprovalRequired) {
        result.errcode = errc::kInvalidRequest;
        result.errmsg = "group is public";
        return result;
    }

    if (isMember(uid, group_id)) {
        result.errcode = errc::kAlreadyInGroup;
        result.errmsg = "already in group";
        return result;
    }

    if (hasPendingRequest(group_id, uid)) {
        result.errcode = errc::kJoinPending;
        result.errmsg = "join request already pending";
        return result;
    }

    if (countUserGroups(uid) >= kMaxGroupsPerUser) {
        result.errcode = errc::kUserGroupLimit;
        result.errmsg = "user group limit reached";
        return result;
    }

    const char* sql =
        "INSERT INTO group_join_request (group_id, applicant_uid, status, created_at) "
        "VALUES (?, ?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(asDb(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        result.errcode = errc::kInternalError;
        result.errmsg = sqlite3_errmsg(asDb(db_));
        return result;
    }

    sqlite3_bind_int(stmt, 1, group_id);
    sqlite3_bind_int(stmt, 2, uid);
    sqlite3_bind_int(stmt, 3, kRequestPending);
    sqlite3_bind_int64(stmt, 4, nowUnix());

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        result.errcode = errc::kInternalError;
        result.errmsg = sqlite3_errmsg(asDb(db_));
        sqlite3_finalize(stmt);
        return result;
    }

    sqlite3_finalize(stmt);

    result.errcode = errc::kOk;
    result.errmsg = "join request submitted";
    result.pending = true;
    return result;
}

GroupOpResult GroupStore::approveJoin(int owner_uid, int group_id, int applicant_uid) {
    GroupOpResult result;

    if (!groupExists(group_id)) {
        result.errcode = errc::kGroupNotFound;
        result.errmsg = "group not found";
        return result;
    }

    if (!isOwner(owner_uid, group_id)) {
        result.errcode = errc::kNotGroupOwner;
        result.errmsg = "not group owner";
        return result;
    }

    if (!hasPendingRequest(group_id, applicant_uid)) {
        result.errcode = errc::kJoinNotPending;
        result.errmsg = "no pending request";
        return result;
    }

    const auto added = addMember(applicant_uid, group_id);
    if (added.errcode != errc::kOk) {
        return added;
    }

    const char* sql =
        "UPDATE group_join_request SET status = ? "
        "WHERE group_id = ? AND applicant_uid = ? AND status = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(asDb(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        result.errcode = errc::kInternalError;
        result.errmsg = sqlite3_errmsg(asDb(db_));
        return result;
    }

    sqlite3_bind_int(stmt, 1, kRequestApproved);
    sqlite3_bind_int(stmt, 2, group_id);
    sqlite3_bind_int(stmt, 3, applicant_uid);
    sqlite3_bind_int(stmt, 4, kRequestPending);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    result.errcode = errc::kOk;
    result.errmsg = "approved";
    result.joined = true;
    return result;
}

GroupOpResult GroupStore::rejectJoin(int owner_uid, int group_id, int applicant_uid) {
    GroupOpResult result;

    if (!groupExists(group_id)) {
        result.errcode = errc::kGroupNotFound;
        result.errmsg = "group not found";
        return result;
    }

    if (!isOwner(owner_uid, group_id)) {
        result.errcode = errc::kNotGroupOwner;
        result.errmsg = "not group owner";
        return result;
    }

    if (!hasPendingRequest(group_id, applicant_uid)) {
        result.errcode = errc::kJoinNotPending;
        result.errmsg = "no pending request";
        return result;
    }

    const char* sql =
        "UPDATE group_join_request SET status = ? "
        "WHERE group_id = ? AND applicant_uid = ? AND status = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(asDb(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        result.errcode = errc::kInternalError;
        result.errmsg = sqlite3_errmsg(asDb(db_));
        return result;
    }

    sqlite3_bind_int(stmt, 1, kRequestRejected);
    sqlite3_bind_int(stmt, 2, group_id);
    sqlite3_bind_int(stmt, 3, applicant_uid);
    sqlite3_bind_int(stmt, 4, kRequestPending);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    result.errcode = errc::kOk;
    result.errmsg = "rejected";
    return result;
}

GroupOpResult GroupStore::leaveGroup(int uid, int group_id) {
    GroupOpResult result;

    if (!groupExists(group_id)) {
        result.errcode = errc::kGroupNotFound;
        result.errmsg = "group not found";
        return result;
    }

    if (!isMember(uid, group_id)) {
        result.errcode = errc::kNotGroupMember;
        result.errmsg = "not a member";
        return result;
    }

    const char* sql = "DELETE FROM group_member WHERE group_id = ? AND user_id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(asDb(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        result.errcode = errc::kInternalError;
        result.errmsg = sqlite3_errmsg(asDb(db_));
        return result;
    }

    sqlite3_bind_int(stmt, 1, group_id);
    sqlite3_bind_int(stmt, 2, uid);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    result.errcode = errc::kOk;
    result.errmsg = "left group";
    return result;
}

std::vector<int> GroupStore::listMemberUids(int group_id) const {
    const char* sql = "SELECT user_id FROM group_member WHERE group_id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(asDb(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return {};
    }

    sqlite3_bind_int(stmt, 1, group_id);

    std::vector<int> uids;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        uids.push_back(sqlite3_column_int(stmt, 0));
    }

    sqlite3_finalize(stmt);
    return uids;
}

std::vector<GroupSummary> GroupStore::listMyGroups(int uid) const {
    const char* sql =
        "SELECT g.id, g.name, g.join_mode, g.owner_uid "
        "FROM chat_group g "
        "INNER JOIN group_member m ON g.id = m.group_id "
        "WHERE m.user_id = ? ORDER BY g.id ASC;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(asDb(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return {};
    }

    sqlite3_bind_int(stmt, 1, uid);

    std::vector<GroupSummary> groups;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        GroupSummary summary;
        summary.group_id = sqlite3_column_int(stmt, 0);
        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        if (name) {
            summary.name = name;
        }
        summary.join_mode = static_cast<StoreJoinMode>(sqlite3_column_int(stmt, 2));
        summary.is_owner = sqlite3_column_int(stmt, 3) == uid;
        groups.push_back(std::move(summary));
    }

    sqlite3_finalize(stmt);
    return groups;
}

std::vector<JoinRequestRow> GroupStore::listPendingRequests(int group_id) const {
    const char* sql =
        "SELECT id, applicant_uid FROM group_join_request "
        "WHERE group_id = ? AND status = ? ORDER BY id ASC;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(asDb(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return {};
    }

    sqlite3_bind_int(stmt, 1, group_id);
    sqlite3_bind_int(stmt, 2, kRequestPending);

    std::vector<JoinRequestRow> rows;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        JoinRequestRow row;
        row.request_id = sqlite3_column_int64(stmt, 0);
        row.applicant_uid = sqlite3_column_int(stmt, 1);
        rows.push_back(row);
    }

    sqlite3_finalize(stmt);
    return rows;
}
