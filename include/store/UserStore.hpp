#ifndef USERSTORE_HPP
#define USERSTORE_HPP

// v0.2.0 — 用户表；id 列即全局唯一 uid（注册成功后分配）

#include <cstdint>
#include <optional>
#include <string>

struct UserRecord {
    int uid{0};
    std::string name;
    std::string password_hash;
    std::string state;
};

struct CreateUserResult {
    int32_t errcode{0};
    int uid{0};
    std::string errmsg;
};

struct UpdatePasswordResult {
    int32_t errcode{0};
    std::string errmsg;
};

class UserStore {
public:
    bool init(const std::string& db_path);

    CreateUserResult createUser(const std::string& name, const std::string& plain_password);

    std::optional<UserRecord> findByName(const std::string& name);

    std::optional<UserRecord> findByUid(int uid);

    bool uidExists(int uid);

    UpdatePasswordResult updatePassword(const std::string& name,
                                      const std::string& old_plain_password,
                                      const std::string& new_plain_password);

private:
    void* db_{nullptr};
};

#endif
