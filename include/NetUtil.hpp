#ifndef NETUTIL_HPP
#define NETUTIL_HPP

// v0.2.0 — 同步 TCP 客户端工具（chat_cli / proto_client 共用帧读写）

#include "Codec.hpp"
#include "chat.pb.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

namespace netutil {

inline bool sendAll(int fd, const std::string& data) {
    std::size_t sent = 0;
    while (sent < data.size()) {
        const ssize_t n = ::send(fd, data.data() + sent, data.size() - sent, 0);
        if (n <= 0) {
            return false;
        }
        sent += static_cast<std::size_t>(n);
    }
    return true;
}

inline bool recvAll(int fd, std::string& out, std::size_t n) {
    out.resize(n);
    std::size_t got = 0;
    while (got < n) {
        const ssize_t r = ::recv(fd, out.data() + got, n - got, 0);
        if (r <= 0) {
            return false;
        }
        got += static_cast<std::size_t>(r);
    }
    return true;
}

inline bool recvFrame(int fd, std::string& body) {
    std::string header;
    if (!recvAll(fd, header, protocol::kHeaderSize)) {
        return false;
    }

    std::uint32_t net_len = 0;
    std::memcpy(&net_len, header.data(), protocol::kHeaderSize);
    const std::uint32_t len = ntohl(net_len);
    if (len > protocol::kMaxBodySize) {
        return false;
    }

    return recvAll(fd, body, len);
}

inline bool sendEnvelope(int fd, const chat::ChatEnvelope& envelope) {
    return sendAll(fd, protocol::pack(envelope.SerializeAsString()));
}

// 仅当 socket 上已有完整一帧时读取（用于菜单循环前 drain 推送）
inline bool tryRecvFrame(int fd, std::string& body) {
    int available = 0;
    if (::ioctl(fd, FIONREAD, &available) < 0 || available < static_cast<int>(protocol::kHeaderSize)) {
        return false;
    }

    char header[protocol::kHeaderSize]{};
    const ssize_t peeked =
        ::recv(fd, header, protocol::kHeaderSize, MSG_PEEK);
    if (peeked != static_cast<ssize_t>(protocol::kHeaderSize)) {
        return false;
    }

    std::uint32_t net_len = 0;
    std::memcpy(&net_len, header, protocol::kHeaderSize);
    const std::uint32_t len = ntohl(net_len);
    if (len > protocol::kMaxBodySize) {
        return false;
    }

    const std::size_t frame_size = protocol::kHeaderSize + len;
    if (static_cast<std::size_t>(available) < frame_size) {
        return false;
    }

    return recvFrame(fd, body);
}

inline void drainNotifications(int fd) {
    std::string body;
    while (tryRecvFrame(fd, body)) {
        chat::ChatEnvelope env;
        if (!env.ParseFromString(body)) {
            continue;
        }
        if (env.msgid() != chat::ONE_CHAT_MSG) {
            continue;
        }
        chat::OneChatNotify notify;
        if (!notify.ParseFromString(env.payload())) {
            continue;
        }
        std::cout << "[push] from=" << notify.from_name() << " (uid=" << notify.from_uid()
                  << ") msg=\"" << notify.content() << "\" id=" << notify.msg_id() << '\n';
    }
}

}  // namespace netutil

#endif
