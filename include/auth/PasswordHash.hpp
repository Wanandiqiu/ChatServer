#ifndef PASSWORDHASH_HPP
#define PASSWORDHASH_HPP

// v0.2.0 — libsodium Argon2id 密码哈希与校验

#include <string>

namespace auth {

std::string hashPassword(const std::string& plain_password);

bool verifyPassword(const std::string& plain_password, const std::string& encoded_hash);

}  // namespace auth

#endif
