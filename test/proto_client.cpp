// 简易 Protobuf 测试客户端：验证长度前缀 + ChatEnvelope
// 用法: ./bin/proto_client [host] [port]
// 默认连接 127.0.0.1:6000，发送登录请求后读取响应

#include "Codec.hpp"
#include "chat.pb.h"

#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>

namespace {

bool sendAll(int fd, const std::string& data) {
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

bool recvAll(int fd, std::string& out, std::size_t n) {
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

bool recvFrame(int fd, std::string& body) {
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

}  // namespace

int main(int argc, char* argv[]) {
    const char* host = "127.0.0.1";
    const int port = (argc >= 3) ? std::stoi(argv[2]) : 6000;
    if (argc >= 2) {
        host = argv[1];
    }

    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    if (::inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
        std::cerr << "invalid host\n";
        return 1;
    }

    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        perror("connect");
        return 1;
    }

    chat::LoginReq login_req;
    login_req.set_username("demo");
    login_req.set_password("123456");

    chat::ChatEnvelope req;
    req.set_msgid(chat::LOGIN_MSG);
    req.set_payload(login_req.SerializeAsString());

    if (!sendAll(fd, protocol::pack(req.SerializeAsString()))) {
        std::cerr << "send failed\n";
        return 1;
    }

    std::string frame_body;
    if (!recvFrame(fd, frame_body)) {
        std::cerr << "recv failed\n";
        return 1;
    }

    chat::ChatEnvelope rsp_envelope;
    if (!rsp_envelope.ParseFromString(frame_body)) {
        std::cerr << "parse envelope failed\n";
        return 1;
    }

    chat::LoginRsp login_rsp;
    if (!login_rsp.ParseFromString(rsp_envelope.payload())) {
        std::cerr << "parse LoginRsp failed\n";
        return 1;
    }

    std::cout << "msgid=" << rsp_envelope.msgid()
              << " errcode=" << login_rsp.errcode()
              << " uid=" << login_rsp.uid()
              << " name=" << login_rsp.name()
              << " errmsg=" << login_rsp.errmsg() << '\n';

    ::close(fd);
    return 0;
}
