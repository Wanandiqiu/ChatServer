#ifndef ONLINEREGISTRY_HPP
#define ONLINEREGISTRY_HPP

// v0.2.0 — 已登录用户的在线 Session 索引（uid -> weak_ptr<Session>）

#include <memory>
#include <unordered_map>

class Session;

class OnlineRegistry {
public:
    void bind(int uid, const std::shared_ptr<Session>& session);

    void unbind(const std::shared_ptr<Session>& session);

    void unbindByUserId(int uid);

    std::shared_ptr<Session> find(int uid);

    void kickExisting(int uid, const std::shared_ptr<Session>& except);

private:
    std::unordered_map<int, std::weak_ptr<Session>> online_;
};

#endif
