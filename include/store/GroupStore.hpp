#ifndef GROUPSTORE_HPP
#define GROUPSTORE_HPP

// v0.4.0 — 群组元数据、成员、入群申请

#include <cstdint>
#include <string>
#include <vector>

constexpr int kMaxGroupMembers = 200;
constexpr int kMaxGroupsPerUser = 50;

enum class StoreJoinMode : int {
    Public = 1,
    ApprovalRequired = 2,
};

struct GroupOpResult {
    int32_t errcode{0};
    std::string errmsg;
    int32_t group_id{0};
    bool joined{false};
    bool pending{false};
};

struct GroupSummary {
    int32_t group_id{0};
    std::string name;
    StoreJoinMode join_mode{StoreJoinMode::Public};
    bool is_owner{false};
};

struct JoinRequestRow {
    int64_t request_id{0};
    int32_t applicant_uid{0};
};

class GroupStore {
public:
    bool init(const std::string& db_path);

    GroupOpResult createGroup(int owner_uid,
                              const std::string& name,
                              const std::string& description,
                              StoreJoinMode join_mode);

    GroupOpResult joinPublic(int uid, int group_id);
    GroupOpResult requestJoin(int uid, int group_id);
    GroupOpResult approveJoin(int owner_uid, int group_id, int applicant_uid);
    GroupOpResult rejectJoin(int owner_uid, int group_id, int applicant_uid);
    GroupOpResult leaveGroup(int uid, int group_id);

    bool groupExists(int group_id) const;
    bool isMember(int uid, int group_id) const;
    bool isOwner(int uid, int group_id) const;
    StoreJoinMode getJoinMode(int group_id) const;
    std::string getGroupName(int group_id) const;

    int countMembers(int group_id) const;
    int countUserGroups(int uid) const;

    std::vector<int> listMemberUids(int group_id) const;
    std::vector<GroupSummary> listMyGroups(int uid) const;
    std::vector<JoinRequestRow> listPendingRequests(int group_id) const;

private:
    GroupOpResult addMember(int uid, int group_id);
    bool hasPendingRequest(int group_id, int applicant_uid) const;

    void* db_{nullptr};
};

#endif
