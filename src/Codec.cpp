#include "Codec.hpp"

#include <arpa/inet.h>
#include <cstring>
#include <stdexcept>

namespace protocol {

std::string pack(const std::string& body) {
    if (body.size() > kMaxBodySize) {
        throw std::runtime_error("message body too large");
    }

    const auto len = static_cast<std::uint32_t>(body.size());
    const std::uint32_t net_len = htonl(len);

    std::string frame;
    frame.reserve(kHeaderSize + body.size());
    frame.append(reinterpret_cast<const char*>(&net_len), kHeaderSize);
    frame.append(body);
    return frame;
}

bool try_unpack(std::string& buffer, std::string& body) {
    if (buffer.size() < kHeaderSize) {
        return false;
    }

    std::uint32_t net_len = 0;
    std::memcpy(&net_len, buffer.data(), kHeaderSize);
    const std::uint32_t len = ntohl(net_len);

    if (len > kMaxBodySize) {
        throw std::runtime_error("invalid frame length");
    }

    if (buffer.size() < kHeaderSize + len) {
        return false;
    }

    body.assign(buffer.data() + kHeaderSize, len);
    buffer.erase(0, kHeaderSize + len);
    return true;
}

}  // namespace protocol
