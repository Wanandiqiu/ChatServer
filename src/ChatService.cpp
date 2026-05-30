// ChatService：账号 + 好友单聊 + 群聊

#include "ChatService.hpp"

#include "ErrCode.hpp"
#include "Session.hpp"
#include "auth/PasswordHash.hpp"

#include <iostream>

namespace {

bool allowsAnonymous(chat::MsgType msgid) {
    return msgid == chat::REG_MSG || msgid == chat::LOGIN_MSG ||
           msgid == chat::RESET_PASSWORD_MSG || msgid == chat::SWITCH_ACCOUNT_MSG;
}

void sendNotLoggedIn(const std::shared_ptr<Session>& session, chat::MsgType req) {
    switch (req) {
        case chat::LOGINOUT_MSG: {
            chat::LogoutRsp rsp;
            rsp.set_errcode(errc::kNotLoggedIn);
            rsp.set_errmsg("login required");
            chat::ChatEnvelope out;
            out.set_msgid(chat::LOGINOUT_MSG_ACK);
            out.set_payload(rsp.SerializeAsString());
            session->send(out);
            return;
        }
        case chat::ADD_FRIEND_MSG: {
            chat::AddFriendRsp rsp;
            rsp.set_errcode(errc::kNotLoggedIn);
            rsp.set_errmsg("login required");
            chat::ChatEnvelope out;
            out.set_msgid(chat::ADD_FRIEND_MSG_ACK);
            out.set_payload(rsp.SerializeAsString());
            session->send(out);
            return;
        }
        case chat::ONE_CHAT_MSG: {
            chat::OneChatRsp rsp;
            rsp.set_errcode(errc::kNotLoggedIn);
            rsp.set_errmsg("login required");
            chat::ChatEnvelope out;
            out.set_msgid(chat::ONE_CHAT_MSG);
            out.set_payload(rsp.SerializeAsString());
            session->send(out);
            return;
        }
        case chat::FETCH_HISTORY_MSG: {
            chat::FetchHistoryRsp rsp;
            rsp.set_errcode(errc::kNotLoggedIn);
            rsp.set_errmsg("login required");
            chat::ChatEnvelope out;
            out.set_msgid(chat::FETCH_HISTORY_MSG_ACK);
            out.set_payload(rsp.SerializeAsString());
            session->send(out);
            return;
        }
        case chat::CREATE_GROUP_MSG: {
            chat::CreateGroupRsp rsp;
            rsp.set_errcode(errc::kNotLoggedIn);
            rsp.set_errmsg("login required");
            chat::ChatEnvelope out;
            out.set_msgid(chat::CREATE_GROUP_MSG_ACK);
            out.set_payload(rsp.SerializeAsString());
            session->send(out);
            return;
        }
        case chat::ADD_GROUP_MSG: {
            chat::JoinGroupRsp rsp;
            rsp.set_errcode(errc::kNotLoggedIn);
            rsp.set_errmsg("login required");
            chat::ChatEnvelope out;
            out.set_msgid(chat::ADD_GROUP_MSG_ACK);
            out.set_payload(rsp.SerializeAsString());
            session->send(out);
            return;
        }
        case chat::GROUP_CHAT_MSG: {
            chat::GroupChatRsp rsp;
            rsp.set_errcode(errc::kNotLoggedIn);
            rsp.set_errmsg("login required");
            chat::ChatEnvelope out;
            out.set_msgid(chat::GROUP_CHAT_MSG);
            out.set_payload(rsp.SerializeAsString());
            session->send(out);
            return;
        }
        case chat::LIST_MY_GROUPS_MSG: {
            chat::ListMyGroupsRsp rsp;
            rsp.set_errcode(errc::kNotLoggedIn);
            rsp.set_errmsg("login required");
            chat::ChatEnvelope out;
            out.set_msgid(chat::LIST_MY_GROUPS_MSG_ACK);
            out.set_payload(rsp.SerializeAsString());
            session->send(out);
            return;
        }
        case chat::FETCH_GROUP_HISTORY_MSG: {
            chat::FetchGroupHistoryRsp rsp;
            rsp.set_errcode(errc::kNotLoggedIn);
            rsp.set_errmsg("login required");
            chat::ChatEnvelope out;
            out.set_msgid(chat::FETCH_GROUP_HISTORY_MSG_ACK);
            out.set_payload(rsp.SerializeAsString());
            session->send(out);
            return;
        }
        case chat::LEAVE_GROUP_MSG: {
            chat::LeaveGroupRsp rsp;
            rsp.set_errcode(errc::kNotLoggedIn);
            rsp.set_errmsg("login required");
            chat::ChatEnvelope out;
            out.set_msgid(chat::LEAVE_GROUP_MSG_ACK);
            out.set_payload(rsp.SerializeAsString());
            session->send(out);
            return;
        }
        case chat::LIST_JOIN_REQUESTS_MSG: {
            chat::ListJoinRequestsRsp rsp;
            rsp.set_errcode(errc::kNotLoggedIn);
            rsp.set_errmsg("login required");
            chat::ChatEnvelope out;
            out.set_msgid(chat::LIST_JOIN_REQUESTS_MSG_ACK);
            out.set_payload(rsp.SerializeAsString());
            session->send(out);
            return;
        }
        case chat::REVIEW_JOIN_REQUEST_MSG: {
            chat::ReviewJoinRequestRsp rsp;
            rsp.set_errcode(errc::kNotLoggedIn);
            rsp.set_errmsg("login required");
            chat::ChatEnvelope out;
            out.set_msgid(chat::REVIEW_JOIN_REQUEST_MSG_ACK);
            out.set_payload(rsp.SerializeAsString());
            session->send(out);
            return;
        }
        default: {
            chat::CommonRsp rsp;
            rsp.set_errcode(errc::kNotLoggedIn);
            rsp.set_errmsg("login required");
            chat::ChatEnvelope out;
            out.set_msgid(chat::MSG_TYPE_UNSPECIFIED);
            out.set_payload(rsp.SerializeAsString());
            session->send(out);
            return;
        }
    }
}

constexpr int kHistoryDefaultLimit = 20;
constexpr int kHistoryMaxLimit = 100;

int clampHistoryLimit(int limit) {
    if (limit <= 0) {
        return kHistoryDefaultLimit;
    }
    if (limit > kHistoryMaxLimit) {
        return kHistoryMaxLimit;
    }
    return limit;
}

StoreJoinMode toStoreJoinMode(chat::JoinMode mode) {
    if (mode == chat::JOIN_APPROVAL_REQUIRED) {
        return StoreJoinMode::ApprovalRequired;
    }
    return StoreJoinMode::Public;
}

chat::JoinMode toProtoJoinMode(StoreJoinMode mode) {
    return mode == StoreJoinMode::ApprovalRequired ? chat::JOIN_APPROVAL_REQUIRED
                                                   : chat::JOIN_PUBLIC;
}

}  // namespace

ChatService& ChatService::instance() {
    static ChatService service;
    return service;
}

ChatService::ChatService() {
    msg_handler_map_.emplace(static_cast<int>(chat::LOGIN_MSG),
        [this](const std::shared_ptr<Session>& session, const chat::ChatEnvelope& envelope) {
            login(session, envelope);
        });
    msg_handler_map_.emplace(static_cast<int>(chat::REG_MSG),
        [this](const std::shared_ptr<Session>& session, const chat::ChatEnvelope& envelope) {
            reg(session, envelope);
        });
    msg_handler_map_.emplace(static_cast<int>(chat::LOGINOUT_MSG),
        [this](const std::shared_ptr<Session>& session, const chat::ChatEnvelope& envelope) {
            logout(session, envelope);
        });
    msg_handler_map_.emplace(static_cast<int>(chat::RESET_PASSWORD_MSG),
        [this](const std::shared_ptr<Session>& session, const chat::ChatEnvelope& envelope) {
            resetPassword(session, envelope);
        });
    msg_handler_map_.emplace(static_cast<int>(chat::SWITCH_ACCOUNT_MSG),
        [this](const std::shared_ptr<Session>& session, const chat::ChatEnvelope& envelope) {
            switchAccount(session, envelope);
        });
    msg_handler_map_.emplace(static_cast<int>(chat::ADD_FRIEND_MSG),
        [this](const std::shared_ptr<Session>& session, const chat::ChatEnvelope& envelope) {
            addFriend(session, envelope);
        });
    msg_handler_map_.emplace(static_cast<int>(chat::ONE_CHAT_MSG),
        [this](const std::shared_ptr<Session>& session, const chat::ChatEnvelope& envelope) {
            oneChat(session, envelope);
        });
    msg_handler_map_.emplace(static_cast<int>(chat::FETCH_HISTORY_MSG),
        [this](const std::shared_ptr<Session>& session, const chat::ChatEnvelope& envelope) {
            fetchHistory(session, envelope);
        });
    msg_handler_map_.emplace(static_cast<int>(chat::CREATE_GROUP_MSG),
        [this](const std::shared_ptr<Session>& session, const chat::ChatEnvelope& envelope) {
            createGroup(session, envelope);
        });
    msg_handler_map_.emplace(static_cast<int>(chat::ADD_GROUP_MSG),
        [this](const std::shared_ptr<Session>& session, const chat::ChatEnvelope& envelope) {
            joinGroup(session, envelope);
        });
    msg_handler_map_.emplace(static_cast<int>(chat::GROUP_CHAT_MSG),
        [this](const std::shared_ptr<Session>& session, const chat::ChatEnvelope& envelope) {
            groupChat(session, envelope);
        });
    msg_handler_map_.emplace(static_cast<int>(chat::LIST_MY_GROUPS_MSG),
        [this](const std::shared_ptr<Session>& session, const chat::ChatEnvelope& envelope) {
            listMyGroups(session, envelope);
        });
    msg_handler_map_.emplace(static_cast<int>(chat::FETCH_GROUP_HISTORY_MSG),
        [this](const std::shared_ptr<Session>& session, const chat::ChatEnvelope& envelope) {
            fetchGroupHistory(session, envelope);
        });
    msg_handler_map_.emplace(static_cast<int>(chat::LEAVE_GROUP_MSG),
        [this](const std::shared_ptr<Session>& session, const chat::ChatEnvelope& envelope) {
            leaveGroup(session, envelope);
        });
    msg_handler_map_.emplace(static_cast<int>(chat::LIST_JOIN_REQUESTS_MSG),
        [this](const std::shared_ptr<Session>& session, const chat::ChatEnvelope& envelope) {
            listJoinRequests(session, envelope);
        });
    msg_handler_map_.emplace(static_cast<int>(chat::REVIEW_JOIN_REQUEST_MSG),
        [this](const std::shared_ptr<Session>& session, const chat::ChatEnvelope& envelope) {
            reviewJoinRequest(session, envelope);
        });
}

bool ChatService::init(const std::string& db_path) {
    ready_ = user_store_.init(db_path) && friend_store_.init(db_path) &&
             message_store_.init(db_path) && group_store_.init(db_path) &&
             group_message_store_.init(db_path);
    return ready_;
}

void ChatService::onMessage(const std::shared_ptr<Session>& session, const std::string& frame_body) {
    if (!ready_) {
        std::cerr << "ChatService not initialized\n";
        return;
    }

    chat::ChatEnvelope envelope;
    if (!envelope.ParseFromString(frame_body)) {
        std::cerr << "failed to parse ChatEnvelope\n";
        return;
    }

    if (!allowsAnonymous(envelope.msgid()) && session->uid() == 0) {
        sendNotLoggedIn(session, envelope.msgid());
        return;
    }

    const auto handler = getHandler(envelope.msgid());
    handler(session, envelope);
}

void ChatService::onDisconnect(const std::shared_ptr<Session>& session) {
    if (session->uid() != 0) {
        std::cout << "user disconnected, uid=" << session->uid() << '\n';
        online_.unbind(session);
        session->resetAuth();
    }
}

MsgHandler ChatService::getHandler(chat::MsgType msgid) {
    const auto it = msg_handler_map_.find(static_cast<int>(msgid));
    if (it == msg_handler_map_.end()) {
        return [this](const std::shared_ptr<Session>& session, const chat::ChatEnvelope& envelope) {
            notImplemented(session, envelope);
        };
    }
    return it->second;
}

void ChatService::clearSessionAuth(const std::shared_ptr<Session>& session) {
    if (session->uid() != 0) {
        online_.unbind(session);
        session->resetAuth();
    }
}

void ChatService::establishUserSession(const std::shared_ptr<Session>& session,
                                       const UserRecord& user) {
    online_.kickExisting(user.uid, session);
    session->setUid(user.uid);
    online_.bind(user.uid, session);
    deliverOfflineMessages(session, user.uid);
    deliverOfflineGroupMessages(session, user.uid);
}

void ChatService::login(const std::shared_ptr<Session>& session, const chat::ChatEnvelope& envelope) {
    chat::LoginReq req;
    if (!req.ParseFromString(envelope.payload())) {
        sendCommonError(session, chat::LOGIN_MSG_ACK, errc::kInvalidRequest, "invalid LoginReq");
        return;
    }

    chat::LoginRsp rsp;

    if (session->uid() != 0) {
        rsp.set_errcode(errc::kAlreadyLoggedIn);
        rsp.set_errmsg("use switch account or logout first");
        chat::ChatEnvelope out;
        out.set_msgid(chat::LOGIN_MSG_ACK);
        out.set_payload(rsp.SerializeAsString());
        sendEnvelope(session, out);
        return;
    }

    if (req.username().empty()) {
        rsp.set_errcode(errc::kInvalidRequest);
        rsp.set_errmsg("username required");
        chat::ChatEnvelope out;
        out.set_msgid(chat::LOGIN_MSG_ACK);
        out.set_payload(rsp.SerializeAsString());
        sendEnvelope(session, out);
        return;
    }

    const auto user = user_store_.findByName(req.username());
    if (!user) {
        rsp.set_errcode(errc::kUserNotFound);
        rsp.set_errmsg("user not found");
        chat::ChatEnvelope out;
        out.set_msgid(chat::LOGIN_MSG_ACK);
        out.set_payload(rsp.SerializeAsString());
        sendEnvelope(session, out);
        return;
    }

    if (!auth::verifyPassword(req.password(), user->password_hash)) {
        rsp.set_errcode(errc::kWrongPassword);
        rsp.set_errmsg("wrong password");
        chat::ChatEnvelope out;
        out.set_msgid(chat::LOGIN_MSG_ACK);
        out.set_payload(rsp.SerializeAsString());
        sendEnvelope(session, out);
        return;
    }

    establishUserSession(session, *user);

    rsp.set_errcode(errc::kOk);
    rsp.set_uid(user->uid);
    rsp.set_name(user->name);
    rsp.set_errmsg("login ok");

    chat::ChatEnvelope out;
    out.set_msgid(chat::LOGIN_MSG_ACK);
    out.set_payload(rsp.SerializeAsString());
    sendEnvelope(session, out);
}

void ChatService::reg(const std::shared_ptr<Session>& session, const chat::ChatEnvelope& envelope) {
    chat::RegReq req;
    if (!req.ParseFromString(envelope.payload())) {
        chat::RegRsp rsp;
        rsp.set_errcode(errc::kInvalidRequest);
        rsp.set_errmsg("invalid RegReq");
        chat::ChatEnvelope out;
        out.set_msgid(chat::REG_MSG_ACK);
        out.set_payload(rsp.SerializeAsString());
        sendEnvelope(session, out);
        return;
    }

    chat::RegRsp rsp;

    if (session->uid() != 0) {
        rsp.set_errcode(errc::kAlreadyLoggedIn);
        rsp.set_errmsg("logout or switch account before register");
        chat::ChatEnvelope out;
        out.set_msgid(chat::REG_MSG_ACK);
        out.set_payload(rsp.SerializeAsString());
        sendEnvelope(session, out);
        return;
    }

    const auto created = user_store_.createUser(req.name(), req.password());

    rsp.set_errcode(created.errcode);
    rsp.set_uid(created.uid);
    rsp.set_errmsg(created.errmsg);
    rsp.set_logged_in(false);

    if (created.errcode == errc::kOk) {
        const auto user = user_store_.findByUid(created.uid);
        if (user) {
            rsp.set_name(user->name);
            establishUserSession(session, *user);
            rsp.set_logged_in(true);
            rsp.set_errmsg("register ok, auto logged in");
        }
    }

    chat::ChatEnvelope out;
    out.set_msgid(chat::REG_MSG_ACK);
    out.set_payload(rsp.SerializeAsString());
    sendEnvelope(session, out);
}

void ChatService::logout(const std::shared_ptr<Session>& session, const chat::ChatEnvelope& envelope) {
    (void)envelope;

    chat::LogoutRsp rsp;

    if (session->uid() == 0) {
        rsp.set_errcode(errc::kNotLoggedIn);
        rsp.set_errmsg("not logged in");
    } else {
        clearSessionAuth(session);
        rsp.set_errcode(errc::kOk);
        rsp.set_errmsg("logout ok");
    }

    chat::ChatEnvelope out;
    out.set_msgid(chat::LOGINOUT_MSG_ACK);
    out.set_payload(rsp.SerializeAsString());
    sendEnvelope(session, out);
}

void ChatService::resetPassword(const std::shared_ptr<Session>& session,
                                const chat::ChatEnvelope& envelope) {
    chat::ResetPasswordReq req;
    chat::ResetPasswordRsp rsp;

    if (!req.ParseFromString(envelope.payload())) {
        rsp.set_errcode(errc::kInvalidRequest);
        rsp.set_errmsg("invalid ResetPasswordReq");
        chat::ChatEnvelope out;
        out.set_msgid(chat::RESET_PASSWORD_MSG_ACK);
        out.set_payload(rsp.SerializeAsString());
        sendEnvelope(session, out);
        return;
    }

    if (session->uid() != 0) {
        const auto self = user_store_.findByUid(session->uid());
        if (!self || self->name != req.username()) {
            rsp.set_errcode(errc::kInvalidRequest);
            rsp.set_errmsg("can only reset password for logged-in account");
            chat::ChatEnvelope out;
            out.set_msgid(chat::RESET_PASSWORD_MSG_ACK);
            out.set_payload(rsp.SerializeAsString());
            sendEnvelope(session, out);
            return;
        }
    }

    const auto updated = user_store_.updatePassword(
        req.username(), req.old_password(), req.new_password());

    rsp.set_errcode(updated.errcode);
    rsp.set_errmsg(updated.errmsg);

    chat::ChatEnvelope out;
    out.set_msgid(chat::RESET_PASSWORD_MSG_ACK);
    out.set_payload(rsp.SerializeAsString());
    sendEnvelope(session, out);
}

void ChatService::switchAccount(const std::shared_ptr<Session>& session,
                                const chat::ChatEnvelope& envelope) {
    chat::SwitchAccountReq req;
    chat::SwitchAccountRsp rsp;

    if (!req.ParseFromString(envelope.payload())) {
        rsp.set_errcode(errc::kInvalidRequest);
        rsp.set_errmsg("invalid SwitchAccountReq");
        chat::ChatEnvelope out;
        out.set_msgid(chat::SWITCH_ACCOUNT_MSG_ACK);
        out.set_payload(rsp.SerializeAsString());
        sendEnvelope(session, out);
        return;
    }

    if (req.username().empty()) {
        rsp.set_errcode(errc::kInvalidRequest);
        rsp.set_errmsg("username required");
        chat::ChatEnvelope out;
        out.set_msgid(chat::SWITCH_ACCOUNT_MSG_ACK);
        out.set_payload(rsp.SerializeAsString());
        sendEnvelope(session, out);
        return;
    }

    const auto user = user_store_.findByName(req.username());
    if (!user) {
        rsp.set_errcode(errc::kUserNotFound);
        rsp.set_errmsg("user not found");
        chat::ChatEnvelope out;
        out.set_msgid(chat::SWITCH_ACCOUNT_MSG_ACK);
        out.set_payload(rsp.SerializeAsString());
        sendEnvelope(session, out);
        return;
    }

    if (!auth::verifyPassword(req.password(), user->password_hash)) {
        rsp.set_errcode(errc::kWrongPassword);
        rsp.set_errmsg("wrong password");
        chat::ChatEnvelope out;
        out.set_msgid(chat::SWITCH_ACCOUNT_MSG_ACK);
        out.set_payload(rsp.SerializeAsString());
        sendEnvelope(session, out);
        return;
    }

    clearSessionAuth(session);
    establishUserSession(session, *user);

    rsp.set_errcode(errc::kOk);
    rsp.set_uid(user->uid);
    rsp.set_name(user->name);
    rsp.set_errmsg("account switched");

    chat::ChatEnvelope out;
    out.set_msgid(chat::SWITCH_ACCOUNT_MSG_ACK);
    out.set_payload(rsp.SerializeAsString());
    sendEnvelope(session, out);
}

void ChatService::addFriend(const std::shared_ptr<Session>& session, const chat::ChatEnvelope& envelope) {
    chat::AddFriendRsp rsp;

    chat::AddFriendReq req;
    if (!req.ParseFromString(envelope.payload())) {
        rsp.set_errcode(errc::kInvalidRequest);
        rsp.set_errmsg("invalid AddFriendReq");
        chat::ChatEnvelope out;
        out.set_msgid(chat::ADD_FRIEND_MSG_ACK);
        out.set_payload(rsp.SerializeAsString());
        sendEnvelope(session, out);
        return;
    }

    const auto friend_user = user_store_.findByName(req.friend_username());
    if (!friend_user) {
        rsp.set_errcode(errc::kUserNotFound);
        rsp.set_errmsg("friend user not found");
        chat::ChatEnvelope out;
        out.set_msgid(chat::ADD_FRIEND_MSG_ACK);
        out.set_payload(rsp.SerializeAsString());
        sendEnvelope(session, out);
        return;
    }

    const auto op = friend_store_.addFriend(session->uid(), friend_user->uid);
    rsp.set_errcode(op.errcode);
    rsp.set_errmsg(op.errmsg);
    rsp.set_friend_uid(op.friend_uid);
    rsp.set_friend_name(friend_user->name);

    chat::ChatEnvelope out;
    out.set_msgid(chat::ADD_FRIEND_MSG_ACK);
    out.set_payload(rsp.SerializeAsString());
    sendEnvelope(session, out);
}

void ChatService::fetchHistory(const std::shared_ptr<Session>& session,
                               const chat::ChatEnvelope& envelope) {
    chat::FetchHistoryRsp rsp;

    chat::FetchHistoryReq req;
    if (!req.ParseFromString(envelope.payload())) {
        rsp.set_errcode(errc::kInvalidRequest);
        rsp.set_errmsg("invalid FetchHistoryReq");
        chat::ChatEnvelope out;
        out.set_msgid(chat::FETCH_HISTORY_MSG_ACK);
        out.set_payload(rsp.SerializeAsString());
        sendEnvelope(session, out);
        return;
    }

    if (req.peer_uid() <= 0) {
        rsp.set_errcode(errc::kInvalidRequest);
        rsp.set_errmsg("peer_uid required");
        chat::ChatEnvelope out;
        out.set_msgid(chat::FETCH_HISTORY_MSG_ACK);
        out.set_payload(rsp.SerializeAsString());
        sendEnvelope(session, out);
        return;
    }

    if (!user_store_.uidExists(req.peer_uid())) {
        rsp.set_errcode(errc::kInvalidUid);
        rsp.set_errmsg("target uid not registered");
        chat::ChatEnvelope out;
        out.set_msgid(chat::FETCH_HISTORY_MSG_ACK);
        out.set_payload(rsp.SerializeAsString());
        sendEnvelope(session, out);
        return;
    }

    if (!friend_store_.areFriends(session->uid(), req.peer_uid())) {
        rsp.set_errcode(errc::kNotFriend);
        rsp.set_errmsg("not friends");
        chat::ChatEnvelope out;
        out.set_msgid(chat::FETCH_HISTORY_MSG_ACK);
        out.set_payload(rsp.SerializeAsString());
        sendEnvelope(session, out);
        return;
    }

    const int limit = clampHistoryLimit(req.limit());
    const auto messages = message_store_.fetchHistoryBetween(
        session->uid(), req.peer_uid(), limit, req.before_msg_id());

    rsp.set_errcode(errc::kOk);
    rsp.set_errmsg("ok");
    for (const auto& msg : messages) {
        auto* entry = rsp.add_messages();
        entry->set_msg_id(msg.id);
        entry->set_from_uid(msg.from_id);
        entry->set_to_uid(msg.to_id);
        entry->set_content(msg.content);
        entry->set_sent_at(msg.sent_at);
    }

    chat::ChatEnvelope out;
    out.set_msgid(chat::FETCH_HISTORY_MSG_ACK);
    out.set_payload(rsp.SerializeAsString());
    sendEnvelope(session, out);
}

void ChatService::oneChat(const std::shared_ptr<Session>& session, const chat::ChatEnvelope& envelope) {
    chat::OneChatRsp rsp;

    chat::OneChatReq req;
    if (!req.ParseFromString(envelope.payload())) {
        rsp.set_errcode(errc::kInvalidRequest);
        rsp.set_errmsg("invalid OneChatReq");
        chat::ChatEnvelope out;
        out.set_msgid(chat::ONE_CHAT_MSG);
        out.set_payload(rsp.SerializeAsString());
        sendEnvelope(session, out);
        return;
    }

    if (req.to_uid() <= 0 || req.content().empty()) {
        rsp.set_errcode(errc::kInvalidRequest);
        rsp.set_errmsg("to_uid and content required");
        chat::ChatEnvelope out;
        out.set_msgid(chat::ONE_CHAT_MSG);
        out.set_payload(rsp.SerializeAsString());
        sendEnvelope(session, out);
        return;
    }

    if (!user_store_.uidExists(req.to_uid())) {
        rsp.set_errcode(errc::kInvalidUid);
        rsp.set_errmsg("target uid not registered");
        chat::ChatEnvelope out;
        out.set_msgid(chat::ONE_CHAT_MSG);
        out.set_payload(rsp.SerializeAsString());
        sendEnvelope(session, out);
        return;
    }

    if (!friend_store_.areFriends(session->uid(), req.to_uid())) {
        rsp.set_errcode(errc::kNotFriend);
        rsp.set_errmsg("not friends");
        chat::ChatEnvelope out;
        out.set_msgid(chat::ONE_CHAT_MSG);
        out.set_payload(rsp.SerializeAsString());
        sendEnvelope(session, out);
        return;
    }

    const auto saved = message_store_.saveMessage(session->uid(), req.to_uid(), req.content());
    rsp.set_errcode(saved.errcode);
    rsp.set_errmsg(saved.errmsg);
    rsp.set_msg_id(saved.msg_id);

    chat::ChatEnvelope ack;
    ack.set_msgid(chat::ONE_CHAT_MSG);
    ack.set_payload(rsp.SerializeAsString());
    sendEnvelope(session, ack);

    if (saved.errcode != errc::kOk) {
        return;
    }

    ChatMessage stored;
    stored.id = saved.msg_id;
    stored.from_id = session->uid();
    stored.to_id = req.to_uid();
    stored.content = req.content();
    stored.sent_at = saved.sent_at;

    const auto from_user = user_store_.findByUid(session->uid());
    const std::string from_name = from_user ? from_user->name : "";

    if (const auto peer = online_.find(req.to_uid())) {
        pushOneChatNotify(peer, stored, from_name);
        message_store_.markDelivered(stored.id);
    }
}

void ChatService::deliverOfflineMessages(const std::shared_ptr<Session>& session, int uid) {
    const auto pending = message_store_.fetchUndelivered(uid);
    for (const auto& msg : pending) {
        const auto from_user = user_store_.findByUid(msg.from_id);
        const std::string from_name = from_user ? from_user->name : "";
        pushOneChatNotify(session, msg, from_name);
        message_store_.markDelivered(msg.id);
    }
}

void ChatService::pushOneChatNotify(const std::shared_ptr<Session>& session,
                                    const ChatMessage& msg,
                                    const std::string& from_name) {
    chat::OneChatNotify notify;
    notify.set_msg_id(msg.id);
    notify.set_from_uid(msg.from_id);
    notify.set_from_name(from_name);
    notify.set_to_uid(msg.to_id);
    notify.set_content(msg.content);
    notify.set_sent_at(msg.sent_at);

    chat::ChatEnvelope out;
    out.set_msgid(chat::ONE_CHAT_MSG);
    out.set_payload(notify.SerializeAsString());
    sendEnvelope(session, out);
}

void ChatService::createGroup(const std::shared_ptr<Session>& session,
                              const chat::ChatEnvelope& envelope) {
    chat::CreateGroupRsp rsp;

    chat::CreateGroupReq req;
    if (!req.ParseFromString(envelope.payload())) {
        rsp.set_errcode(errc::kInvalidRequest);
        rsp.set_errmsg("invalid CreateGroupReq");
        chat::ChatEnvelope out;
        out.set_msgid(chat::CREATE_GROUP_MSG_ACK);
        out.set_payload(rsp.SerializeAsString());
        sendEnvelope(session, out);
        return;
    }

    const auto op = group_store_.createGroup(
        session->uid(), req.name(), req.description(), toStoreJoinMode(req.join_mode()));

    rsp.set_errcode(op.errcode);
    rsp.set_errmsg(op.errmsg);
    rsp.set_group_id(op.group_id);

    chat::ChatEnvelope out;
    out.set_msgid(chat::CREATE_GROUP_MSG_ACK);
    out.set_payload(rsp.SerializeAsString());
    sendEnvelope(session, out);
}

void ChatService::joinGroup(const std::shared_ptr<Session>& session,
                            const chat::ChatEnvelope& envelope) {
    chat::JoinGroupRsp rsp;

    chat::JoinGroupReq req;
    if (!req.ParseFromString(envelope.payload())) {
        rsp.set_errcode(errc::kInvalidRequest);
        rsp.set_errmsg("invalid JoinGroupReq");
        chat::ChatEnvelope out;
        out.set_msgid(chat::ADD_GROUP_MSG_ACK);
        out.set_payload(rsp.SerializeAsString());
        sendEnvelope(session, out);
        return;
    }

    if (req.group_id() <= 0) {
        rsp.set_errcode(errc::kInvalidRequest);
        rsp.set_errmsg("group_id required");
        chat::ChatEnvelope out;
        out.set_msgid(chat::ADD_GROUP_MSG_ACK);
        out.set_payload(rsp.SerializeAsString());
        sendEnvelope(session, out);
        return;
    }

    GroupOpResult op;
    if (!group_store_.groupExists(req.group_id())) {
        op.errcode = errc::kGroupNotFound;
        op.errmsg = "group not found";
    } else if (group_store_.getJoinMode(req.group_id()) == StoreJoinMode::Public) {
        op = group_store_.joinPublic(session->uid(), req.group_id());
    } else {
        op = group_store_.requestJoin(session->uid(), req.group_id());
    }

    rsp.set_errcode(op.errcode);
    rsp.set_errmsg(op.errmsg);
    rsp.set_joined(op.joined);
    rsp.set_pending(op.pending);

    chat::ChatEnvelope out;
    out.set_msgid(chat::ADD_GROUP_MSG_ACK);
    out.set_payload(rsp.SerializeAsString());
    sendEnvelope(session, out);
}

void ChatService::listMyGroups(const std::shared_ptr<Session>& session,
                               const chat::ChatEnvelope& envelope) {
    (void)envelope;

    chat::ListMyGroupsRsp rsp;
    const auto groups = group_store_.listMyGroups(session->uid());

    rsp.set_errcode(errc::kOk);
    rsp.set_errmsg("ok");
    for (const auto& g : groups) {
        auto* item = rsp.add_groups();
        item->set_group_id(g.group_id);
        item->set_name(g.name);
        item->set_join_mode(toProtoJoinMode(g.join_mode));
        item->set_is_owner(g.is_owner);
    }

    chat::ChatEnvelope out;
    out.set_msgid(chat::LIST_MY_GROUPS_MSG_ACK);
    out.set_payload(rsp.SerializeAsString());
    sendEnvelope(session, out);
}

void ChatService::leaveGroup(const std::shared_ptr<Session>& session,
                             const chat::ChatEnvelope& envelope) {
    chat::LeaveGroupRsp rsp;

    chat::LeaveGroupReq req;
    if (!req.ParseFromString(envelope.payload())) {
        rsp.set_errcode(errc::kInvalidRequest);
        rsp.set_errmsg("invalid LeaveGroupReq");
        chat::ChatEnvelope out;
        out.set_msgid(chat::LEAVE_GROUP_MSG_ACK);
        out.set_payload(rsp.SerializeAsString());
        sendEnvelope(session, out);
        return;
    }

    const auto op = group_store_.leaveGroup(session->uid(), req.group_id());
    rsp.set_errcode(op.errcode);
    rsp.set_errmsg(op.errmsg);

    chat::ChatEnvelope out;
    out.set_msgid(chat::LEAVE_GROUP_MSG_ACK);
    out.set_payload(rsp.SerializeAsString());
    sendEnvelope(session, out);
}

void ChatService::listJoinRequests(const std::shared_ptr<Session>& session,
                                   const chat::ChatEnvelope& envelope) {
    chat::ListJoinRequestsRsp rsp;

    chat::ListJoinRequestsReq req;
    if (!req.ParseFromString(envelope.payload())) {
        rsp.set_errcode(errc::kInvalidRequest);
        rsp.set_errmsg("invalid ListJoinRequestsReq");
        chat::ChatEnvelope out;
        out.set_msgid(chat::LIST_JOIN_REQUESTS_MSG_ACK);
        out.set_payload(rsp.SerializeAsString());
        sendEnvelope(session, out);
        return;
    }

    if (!group_store_.groupExists(req.group_id())) {
        rsp.set_errcode(errc::kGroupNotFound);
        rsp.set_errmsg("group not found");
    } else if (!group_store_.isOwner(session->uid(), req.group_id())) {
        rsp.set_errcode(errc::kNotGroupOwner);
        rsp.set_errmsg("not group owner");
    } else {
        const auto rows = group_store_.listPendingRequests(req.group_id());
        rsp.set_errcode(errc::kOk);
        rsp.set_errmsg("ok");
        for (const auto& row : rows) {
            auto* entry = rsp.add_requests();
            entry->set_request_id(row.request_id);
            entry->set_applicant_uid(row.applicant_uid);
        }
    }

    chat::ChatEnvelope out;
    out.set_msgid(chat::LIST_JOIN_REQUESTS_MSG_ACK);
    out.set_payload(rsp.SerializeAsString());
    sendEnvelope(session, out);
}

void ChatService::reviewJoinRequest(const std::shared_ptr<Session>& session,
                                    const chat::ChatEnvelope& envelope) {
    chat::ReviewJoinRequestRsp rsp;

    chat::ReviewJoinRequestReq req;
    if (!req.ParseFromString(envelope.payload())) {
        rsp.set_errcode(errc::kInvalidRequest);
        rsp.set_errmsg("invalid ReviewJoinRequestReq");
        chat::ChatEnvelope out;
        out.set_msgid(chat::REVIEW_JOIN_REQUEST_MSG_ACK);
        out.set_payload(rsp.SerializeAsString());
        sendEnvelope(session, out);
        return;
    }

    GroupOpResult op;
    if (req.approve()) {
        op = group_store_.approveJoin(session->uid(), req.group_id(), req.applicant_uid());
    } else {
        op = group_store_.rejectJoin(session->uid(), req.group_id(), req.applicant_uid());
    }

    rsp.set_errcode(op.errcode);
    rsp.set_errmsg(op.errmsg);

    chat::ChatEnvelope out;
    out.set_msgid(chat::REVIEW_JOIN_REQUEST_MSG_ACK);
    out.set_payload(rsp.SerializeAsString());
    sendEnvelope(session, out);
}

void ChatService::groupChat(const std::shared_ptr<Session>& session,
                            const chat::ChatEnvelope& envelope) {
    chat::GroupChatRsp rsp;

    chat::GroupChatReq req;
    if (!req.ParseFromString(envelope.payload())) {
        rsp.set_errcode(errc::kInvalidRequest);
        rsp.set_errmsg("invalid GroupChatReq");
        chat::ChatEnvelope out;
        out.set_msgid(chat::GROUP_CHAT_MSG);
        out.set_payload(rsp.SerializeAsString());
        sendEnvelope(session, out);
        return;
    }

    if (req.group_id() <= 0 || req.content().empty()) {
        rsp.set_errcode(errc::kInvalidRequest);
        rsp.set_errmsg("group_id and content required");
        chat::ChatEnvelope out;
        out.set_msgid(chat::GROUP_CHAT_MSG);
        out.set_payload(rsp.SerializeAsString());
        sendEnvelope(session, out);
        return;
    }

    if (!group_store_.groupExists(req.group_id())) {
        rsp.set_errcode(errc::kGroupNotFound);
        rsp.set_errmsg("group not found");
        chat::ChatEnvelope out;
        out.set_msgid(chat::GROUP_CHAT_MSG);
        out.set_payload(rsp.SerializeAsString());
        sendEnvelope(session, out);
        return;
    }

    if (!group_store_.isMember(session->uid(), req.group_id())) {
        rsp.set_errcode(errc::kNotGroupMember);
        rsp.set_errmsg("not a group member");
        chat::ChatEnvelope out;
        out.set_msgid(chat::GROUP_CHAT_MSG);
        out.set_payload(rsp.SerializeAsString());
        sendEnvelope(session, out);
        return;
    }

    const auto saved =
        group_message_store_.saveMessage(session->uid(), req.group_id(), req.content());
    rsp.set_errcode(saved.errcode);
    rsp.set_errmsg(saved.errmsg);
    rsp.set_msg_id(saved.msg_id);

    chat::ChatEnvelope ack;
    ack.set_msgid(chat::GROUP_CHAT_MSG);
    ack.set_payload(rsp.SerializeAsString());
    sendEnvelope(session, ack);

    if (saved.errcode != errc::kOk) {
        return;
    }

    GroupMessage stored;
    stored.id = saved.msg_id;
    stored.group_id = req.group_id();
    stored.from_uid = session->uid();
    stored.content = req.content();
    stored.sent_at = saved.sent_at;

    const auto from_user = user_store_.findByUid(session->uid());
    const std::string from_name = from_user ? from_user->name : "";
    const std::string group_name = group_store_.getGroupName(req.group_id());

    const auto members = group_store_.listMemberUids(req.group_id());
    for (const int member_uid : members) {
        if (member_uid == session->uid()) {
            continue;
        }

        if (const auto peer = online_.find(member_uid)) {
            pushGroupChatNotify(peer, stored, from_name, group_name);
            group_message_store_.insertDelivery(stored.id, member_uid, 1);
        } else {
            group_message_store_.insertDelivery(stored.id, member_uid, 0);
        }
    }
}

void ChatService::fetchGroupHistory(const std::shared_ptr<Session>& session,
                                    const chat::ChatEnvelope& envelope) {
    chat::FetchGroupHistoryRsp rsp;

    chat::FetchGroupHistoryReq req;
    if (!req.ParseFromString(envelope.payload())) {
        rsp.set_errcode(errc::kInvalidRequest);
        rsp.set_errmsg("invalid FetchGroupHistoryReq");
        chat::ChatEnvelope out;
        out.set_msgid(chat::FETCH_GROUP_HISTORY_MSG_ACK);
        out.set_payload(rsp.SerializeAsString());
        sendEnvelope(session, out);
        return;
    }

    if (req.group_id() <= 0) {
        rsp.set_errcode(errc::kInvalidRequest);
        rsp.set_errmsg("group_id required");
        chat::ChatEnvelope out;
        out.set_msgid(chat::FETCH_GROUP_HISTORY_MSG_ACK);
        out.set_payload(rsp.SerializeAsString());
        sendEnvelope(session, out);
        return;
    }

    if (!group_store_.groupExists(req.group_id())) {
        rsp.set_errcode(errc::kGroupNotFound);
        rsp.set_errmsg("group not found");
        chat::ChatEnvelope out;
        out.set_msgid(chat::FETCH_GROUP_HISTORY_MSG_ACK);
        out.set_payload(rsp.SerializeAsString());
        sendEnvelope(session, out);
        return;
    }

    if (!group_store_.isMember(session->uid(), req.group_id())) {
        rsp.set_errcode(errc::kNotGroupMember);
        rsp.set_errmsg("not a group member");
        chat::ChatEnvelope out;
        out.set_msgid(chat::FETCH_GROUP_HISTORY_MSG_ACK);
        out.set_payload(rsp.SerializeAsString());
        sendEnvelope(session, out);
        return;
    }

    const int limit = clampHistoryLimit(req.limit());
    const auto messages = group_message_store_.fetchGroupHistory(
        req.group_id(), limit, req.before_msg_id());

    rsp.set_errcode(errc::kOk);
    rsp.set_errmsg("ok");
    for (const auto& msg : messages) {
        auto* entry = rsp.add_messages();
        entry->set_msg_id(msg.id);
        entry->set_group_id(msg.group_id);
        entry->set_from_uid(msg.from_uid);
        entry->set_content(msg.content);
        entry->set_sent_at(msg.sent_at);
    }

    chat::ChatEnvelope out;
    out.set_msgid(chat::FETCH_GROUP_HISTORY_MSG_ACK);
    out.set_payload(rsp.SerializeAsString());
    sendEnvelope(session, out);
}

void ChatService::deliverOfflineGroupMessages(const std::shared_ptr<Session>& session, int uid) {
    const auto pending = group_message_store_.fetchUndeliveredForUser(uid);
    for (const auto& msg : pending) {
        const auto from_user = user_store_.findByUid(msg.from_uid);
        const std::string from_name = from_user ? from_user->name : "";
        const std::string group_name = group_store_.getGroupName(msg.group_id);
        pushGroupChatNotify(session, msg, from_name, group_name);
        group_message_store_.markDelivered(msg.id, uid);
    }
}

void ChatService::pushGroupChatNotify(const std::shared_ptr<Session>& session,
                                      const GroupMessage& msg,
                                      const std::string& from_name,
                                      const std::string& group_name) {
    chat::GroupChatNotify notify;
    notify.set_msg_id(msg.id);
    notify.set_group_id(msg.group_id);
    notify.set_group_name(group_name);
    notify.set_from_uid(msg.from_uid);
    notify.set_from_name(from_name);
    notify.set_content(msg.content);
    notify.set_sent_at(msg.sent_at);

    chat::ChatEnvelope out;
    out.set_msgid(chat::GROUP_CHAT_MSG);
    out.set_payload(notify.SerializeAsString());
    sendEnvelope(session, out);
}

void ChatService::notImplemented(const std::shared_ptr<Session>& session,
                                 const chat::ChatEnvelope& envelope) {
    std::cerr << "no handler for msgid=" << envelope.msgid() << '\n';
    sendCommonError(session, chat::MSG_TYPE_UNSPECIFIED, errc::kNotImplemented,
                    "message type not implemented");
}

void ChatService::sendCommonError(const std::shared_ptr<Session>& session,
                                  chat::MsgType ack_type,
                                  int32_t errcode,
                                  const std::string& errmsg) {
    chat::CommonRsp rsp;
    rsp.set_errcode(errcode);
    rsp.set_errmsg(errmsg);

    chat::ChatEnvelope out;
    out.set_msgid(ack_type);
    out.set_payload(rsp.SerializeAsString());
    sendEnvelope(session, out);
}

void ChatService::sendEnvelope(const std::shared_ptr<Session>& session,
                               const chat::ChatEnvelope& envelope) {
    session->send(envelope);
}
