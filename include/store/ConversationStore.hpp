#ifndef CONVERSATIONSTORE_HPP
#define CONVERSATIONSTORE_HPP

// v0.4.3 — 收件箱会话状态与未读计数

#include <cstdint>
#include <string>
#include <vector>

enum class ConversationSessionType : int {
    Peer = 1,
    Group = 2,
};

struct ConversationRow {
    ConversationSessionType session_type{ConversationSessionType::Peer};
    int32_t target_id{0};
    std::string title;
    int64_t last_msg_id{0};
    int64_t last_msg_at{0};
    std::string last_preview;
    int64_t last_read_msg_id{0};
    int32_t unread_count{0};
};

class ConversationStore {
public:
    bool init(const std::string& db_path);

    void touchOutgoing(int user_id,
                       ConversationSessionType session_type,
                       int32_t target_id,
                       const std::string& title,
                       const std::string& preview,
                       int64_t msg_id,
                       int64_t sent_at);

    void touchIncoming(int user_id,
                       ConversationSessionType session_type,
                       int32_t target_id,
                       const std::string& title,
                       const std::string& preview,
                       int64_t msg_id,
                       int64_t sent_at);

    std::vector<ConversationRow> listConversations(int user_id) const;

    bool markRead(int user_id,
                  ConversationSessionType session_type,
                  int32_t target_id,
                  int64_t read_through_msg_id);

    bool conversationExists(int user_id,
                            ConversationSessionType session_type,
                            int32_t target_id) const;

private:
    void* db_{nullptr};
};

#endif
