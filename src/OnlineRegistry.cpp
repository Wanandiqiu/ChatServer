// 挤下线：先通知旧 Session（LoginRsp + kKicked），再 shutdown

#include "OnlineRegistry.hpp"

#include "ErrCode.hpp"
#include "Session.hpp"

#include "chat.pb.h"

void OnlineRegistry::bind(int uid, const std::shared_ptr<Session>& session) {
    online_[uid] = session;
}

void OnlineRegistry::unbind(const std::shared_ptr<Session>& session) {
    const int uid = session->uid();
    if (uid == 0) {
        return;
    }

    const auto it = online_.find(uid);
    if (it != online_.end()) {
        if (auto existing = it->second.lock()) {
            if (existing.get() == session.get()) {
                online_.erase(it);
            }
        } else {
            online_.erase(it);
        }
    }
}

void OnlineRegistry::unbindByUserId(int uid) {
    online_.erase(uid);
}

std::shared_ptr<Session> OnlineRegistry::find(int uid) {
    const auto it = online_.find(uid);
    if (it == online_.end()) {
        return nullptr;
    }
    return it->second.lock();
}

void OnlineRegistry::kickExisting(int uid, const std::shared_ptr<Session>& except) {
    const auto existing = find(uid);
    if (!existing || existing.get() == except.get()) {
        return;
    }

    chat::LoginRsp rsp;
    rsp.set_errcode(errc::kKicked);
    rsp.set_errmsg("logged in elsewhere");
    rsp.set_uid(uid);

    chat::ChatEnvelope out;
    out.set_msgid(chat::LOGIN_MSG_ACK);
    out.set_payload(rsp.SerializeAsString());
    existing->send(out);
    existing->shutdown();
    unbind(existing);
}
