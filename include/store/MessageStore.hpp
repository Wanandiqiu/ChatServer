#ifndef MESSAGESTORE_HPP
#define MESSAGESTORE_HPP

// v0.3.0 — 单聊消息持久化与离线未投递队列

#include <cstdint>
#include <string>
#include <vector>

struct ChatMessage {
    int64_t id{0};
    int from_id{0};
    int to_id{0};
    std::string content;
    int64_t sent_at{0};
};

struct SaveMessageResult {
    int32_t errcode{0};
    std::string errmsg;
    int64_t msg_id{0};
    int64_t sent_at{0};
};

class MessageStore {
public:
    bool init(const std::string& db_path);

    SaveMessageResult saveMessage(int from_id, int to_id, const std::string& content);

    std::vector<ChatMessage> fetchUndelivered(int to_id);

    std::vector<ChatMessage> fetchHistoryBetween(int self_uid,
                                                 int peer_uid,
                                                 int limit,
                                                 int64_t before_msg_id);

    bool markDelivered(int64_t msg_id);

private:
    void* db_{nullptr};
};

#endif
