#include "auth/PasswordHash.hpp"

#include <sodium.h>

#include <stdexcept>
#include <string>

namespace auth {

std::string hashPassword(const std::string& plain_password) {
    if (sodium_init() < 0) {
        throw std::runtime_error("libsodium init failed");
    }

    char encoded[crypto_pwhash_STRBYTES]{};
    if (crypto_pwhash_str(
            encoded,
            plain_password.c_str(),
            plain_password.size(),
            crypto_pwhash_OPSLIMIT_MODERATE,
            crypto_pwhash_MEMLIMIT_MODERATE) != 0) {
        throw std::runtime_error("password hash failed");
    }

    return std::string(encoded);
}

bool verifyPassword(const std::string& plain_password, const std::string& encoded_hash) {
    if (sodium_init() < 0) {
        return false;
    }

    return crypto_pwhash_str_verify(
               encoded_hash.c_str(),
               plain_password.c_str(),
               plain_password.size()) == 0;
}

}  // namespace auth
