#ifndef GROUPMESSAGESTORE_HPP
#define GROUPMESSAGESTORE_HPP

// v0.4.0 — 群消息与按成员投递

#include <cstdint>
#include <string>
#include <vector>

struct GroupMessage {
    int64_t id{0};
    int32_t group_id{0};
    int32_t from_uid{0};
    std::string content;
    int64_t sent_at{0};
};

struct SaveGroupMessageResult {
    int32_t errcode{0};
    std::string errmsg;
    int64_t msg_id{0};
    int64_t sent_at{0};
};

class GroupMessageStore {
public:
    bool init(const std::string& db_path);

    SaveGroupMessageResult saveMessage(int from_uid, int group_id, const std::string& content);

    bool insertDelivery(int64_t msg_id, int user_id, int delivered);

    bool markDelivered(int64_t msg_id, int user_id);

    std::vector<GroupMessage> fetchUndeliveredForUser(int user_id);

    std::vector<GroupMessage> fetchGroupHistory(int group_id, int limit, int64_t before_msg_id);

private:
    void* db_{nullptr};
};

#endif
