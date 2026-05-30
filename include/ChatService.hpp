#ifndef CHATSERVICE_HPP
#define CHATSERVICE_HPP

// v0.1.0 — 按 MsgType 分发 ChatEnvelope
// v0.2.0 — 账号体系
// v0.3.x 账号/好友/单聊；v0.4.1 单聊历史；v0.4.2 群聊

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "OnlineRegistry.hpp"
#include "chat.pb.h"
#include "store/FriendStore.hpp"
#include "store/GroupMessageStore.hpp"
#include "store/GroupStore.hpp"
#include "store/MessageStore.hpp"
#include "store/UserStore.hpp"

class Session;

using MsgHandler = std::function<void(const std::shared_ptr<Session>&, const chat::ChatEnvelope&)>;

class ChatService {
public:
    static ChatService& instance();

    bool init(const std::string& db_path);

    void onMessage(const std::shared_ptr<Session>& session, const std::string& frame_body);

    void onDisconnect(const std::shared_ptr<Session>& session);

private:
    ChatService();

    MsgHandler getHandler(chat::MsgType msgid);

    void login(const std::shared_ptr<Session>& session, const chat::ChatEnvelope& envelope);
    void reg(const std::shared_ptr<Session>& session, const chat::ChatEnvelope& envelope);
    void logout(const std::shared_ptr<Session>& session, const chat::ChatEnvelope& envelope);
    void resetPassword(const std::shared_ptr<Session>& session, const chat::ChatEnvelope& envelope);
    void switchAccount(const std::shared_ptr<Session>& session, const chat::ChatEnvelope& envelope);

    void clearSessionAuth(const std::shared_ptr<Session>& session);
    void establishUserSession(const std::shared_ptr<Session>& session, const UserRecord& user);
    void addFriend(const std::shared_ptr<Session>& session, const chat::ChatEnvelope& envelope);
    void oneChat(const std::shared_ptr<Session>& session, const chat::ChatEnvelope& envelope);
    void fetchHistory(const std::shared_ptr<Session>& session, const chat::ChatEnvelope& envelope);

    void createGroup(const std::shared_ptr<Session>& session, const chat::ChatEnvelope& envelope);
    void joinGroup(const std::shared_ptr<Session>& session, const chat::ChatEnvelope& envelope);
    void groupChat(const std::shared_ptr<Session>& session, const chat::ChatEnvelope& envelope);
    void listMyGroups(const std::shared_ptr<Session>& session, const chat::ChatEnvelope& envelope);
    void fetchGroupHistory(const std::shared_ptr<Session>& session, const chat::ChatEnvelope& envelope);
    void leaveGroup(const std::shared_ptr<Session>& session, const chat::ChatEnvelope& envelope);
    void listJoinRequests(const std::shared_ptr<Session>& session, const chat::ChatEnvelope& envelope);
    void reviewJoinRequest(const std::shared_ptr<Session>& session, const chat::ChatEnvelope& envelope);

    void notImplemented(const std::shared_ptr<Session>& session, const chat::ChatEnvelope& envelope);

    void deliverOfflineMessages(const std::shared_ptr<Session>& session, int uid);
    void deliverOfflineGroupMessages(const std::shared_ptr<Session>& session, int uid);

    void pushOneChatNotify(const std::shared_ptr<Session>& session,
                           const ChatMessage& msg,
                           const std::string& from_name);

    void pushGroupChatNotify(const std::shared_ptr<Session>& session,
                             const GroupMessage& msg,
                             const std::string& from_name,
                             const std::string& group_name);

    static void sendEnvelope(const std::shared_ptr<Session>& session, const chat::ChatEnvelope& envelope);

    void sendCommonError(const std::shared_ptr<Session>& session,
                         chat::MsgType ack_type,
                         int32_t errcode,
                         const std::string& errmsg);

    std::unordered_map<int, MsgHandler> msg_handler_map_;
    UserStore user_store_;
    FriendStore friend_store_;
    MessageStore message_store_;
    GroupStore group_store_;
    GroupMessageStore group_message_store_;
    OnlineRegistry online_;
    bool ready_{false};
};

#endif
