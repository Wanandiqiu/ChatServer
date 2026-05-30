#ifndef FRIENDSTORE_HPP
#define FRIENDSTORE_HPP

// v0.3.0 — 好友关系（双向）

#include <cstdint>
#include <string>

struct FriendOpResult {
    int32_t errcode{0};
    std::string errmsg;
    int friend_uid{0};
};

class FriendStore {
public:
    bool init(const std::string& db_path);

    FriendOpResult addFriend(int uid, int friend_uid);

    bool areFriends(int uid, int friend_uid) const;

private:
    void* db_{nullptr};
};

#endif
