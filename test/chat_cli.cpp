// v0.4.1 交互式 CLI：含单聊历史拉取（菜单 9）
// 用法: ./bin/chat_cli [host] [port]

#include "NetUtil.hpp"
#include "chat.pb.h"

#include <iostream>
#include <limits>
#include <string>

namespace {

int connectTo(const char* host, int port) {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    if (::inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
        std::cerr << "invalid host\n";
        ::close(fd);
        return -1;
    }

    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        perror("connect");
        ::close(fd);
        return -1;
    }

    return fd;
}

bool exchange(int fd, const chat::ChatEnvelope& req, chat::ChatEnvelope& rsp) {
    if (!netutil::sendEnvelope(fd, req)) {
        std::cerr << "send failed\n";
        return false;
    }

    std::string frame_body;
    if (!netutil::recvFrame(fd, frame_body)) {
        std::cerr << "recv failed\n";
        return false;
    }

    return rsp.ParseFromString(frame_body);
}

void readLine(const std::string& prompt, std::string& out) {
    std::cout << prompt;
    std::getline(std::cin, out);
}

}  // namespace

int main(int argc, char* argv[]) {
    const char* host = "127.0.0.1";
    const int port = (argc >= 3) ? std::stoi(argv[2]) : 6000;
    if (argc >= 2) {
        host = argv[1];
    }

    std::cout << "Chat CLI -> " << host << ':' << port << '\n';

    int fd = connectTo(host, port);
    if (fd < 0) {
        return 1;
    }

    int logged_in_uid = 0;

    for (;;) {
        netutil::drainNotifications(fd);

        std::cout << "\n1) register  2) login  3) logout  4) quit\n"
                  << "5) add friend  6) send chat\n"
                  << "7) reset password  8) switch account\n"
                  << "9) fetch history\n> ";
        int choice = 0;
        if (!(std::cin >> choice)) {
            break;
        }
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        if (choice == 4) {
            break;
        }

        if (choice == 1) {
            if (logged_in_uid != 0) {
                std::cout << "logout first before register on this connection\n";
                continue;
            }

            std::string name;
            std::string password;
            readLine("username: ", name);
            readLine("password: ", password);

            chat::RegReq req;
            req.set_name(name);
            req.set_password(password);

            chat::ChatEnvelope env;
            env.set_msgid(chat::REG_MSG);
            env.set_payload(req.SerializeAsString());

            chat::ChatEnvelope rsp_env;
            if (!exchange(fd, env, rsp_env)) {
                break;
            }

            chat::RegRsp rsp;
            if (!rsp.ParseFromString(rsp_env.payload())) {
                std::cerr << "parse RegRsp failed\n";
                continue;
            }

            std::cout << "RegRsp errcode=" << rsp.errcode() << " uid=" << rsp.uid()
                      << " name=" << rsp.name() << " logged_in=" << rsp.logged_in()
                      << " errmsg=" << rsp.errmsg() << '\n';
            if (rsp.errcode() == 0 && rsp.logged_in()) {
                logged_in_uid = rsp.uid();
                netutil::drainNotifications(fd);
            }
            continue;
        }

        if (choice == 2) {
            std::string name;
            std::string password;
            readLine("username: ", name);
            readLine("password: ", password);

            chat::LoginReq req;
            req.set_username(name);
            req.set_password(password);

            chat::ChatEnvelope env;
            env.set_msgid(chat::LOGIN_MSG);
            env.set_payload(req.SerializeAsString());

            chat::ChatEnvelope rsp_env;
            if (!exchange(fd, env, rsp_env)) {
                break;
            }

            chat::LoginRsp rsp;
            if (!rsp.ParseFromString(rsp_env.payload())) {
                std::cerr << "parse LoginRsp failed\n";
                continue;
            }

            std::cout << "LoginRsp errcode=" << rsp.errcode() << " uid=" << rsp.uid()
                      << " name=" << rsp.name() << " errmsg=" << rsp.errmsg() << '\n';

            if (rsp.errcode() == 0) {
                logged_in_uid = rsp.uid();
                netutil::drainNotifications(fd);
            } else {
                logged_in_uid = 0;
            }
            continue;
        }

        if (choice == 3) {
            chat::LogoutReq req;
            chat::ChatEnvelope env;
            env.set_msgid(chat::LOGINOUT_MSG);
            env.set_payload(req.SerializeAsString());

            chat::ChatEnvelope rsp_env;
            if (!exchange(fd, env, rsp_env)) {
                break;
            }

            chat::LogoutRsp rsp;
            if (!rsp.ParseFromString(rsp_env.payload())) {
                std::cerr << "parse LogoutRsp failed\n";
                continue;
            }

            std::cout << "LogoutRsp errcode=" << rsp.errcode() << " errmsg=" << rsp.errmsg()
                      << '\n';
            if (rsp.errcode() == 0) {
                logged_in_uid = 0;
            }
            continue;
        }

        if (choice == 5) {
            if (logged_in_uid == 0) {
                std::cout << "please login or register first\n";
                continue;
            }

            std::string friend_name;
            readLine("friend username: ", friend_name);

            chat::AddFriendReq req;
            req.set_friend_username(friend_name);

            chat::ChatEnvelope env;
            env.set_msgid(chat::ADD_FRIEND_MSG);
            env.set_payload(req.SerializeAsString());

            chat::ChatEnvelope rsp_env;
            if (!exchange(fd, env, rsp_env)) {
                break;
            }

            chat::AddFriendRsp rsp;
            if (!rsp.ParseFromString(rsp_env.payload())) {
                std::cerr << "parse AddFriendRsp failed\n";
                continue;
            }

            std::cout << "AddFriendRsp errcode=" << rsp.errcode()
                      << " friend_uid=" << rsp.friend_uid() << " name=" << rsp.friend_name()
                      << " errmsg=" << rsp.errmsg() << '\n';
            continue;
        }

        if (choice == 6) {
            if (logged_in_uid == 0) {
                std::cout << "please login first\n";
                continue;
            }

            std::string to_uid_str;
            std::string content;
            readLine("friend uid: ", to_uid_str);
            int to_uid = 0;
            try {
                to_uid = std::stoi(to_uid_str);
            } catch (...) {
                std::cout << "invalid uid\n";
                continue;
            }
            readLine("message: ", content);

            chat::OneChatReq req;
            req.set_to_uid(to_uid);
            req.set_content(content);

            chat::ChatEnvelope env;
            env.set_msgid(chat::ONE_CHAT_MSG);
            env.set_payload(req.SerializeAsString());

            chat::ChatEnvelope rsp_env;
            if (!exchange(fd, env, rsp_env)) {
                break;
            }

            chat::OneChatRsp rsp;
            if (!rsp.ParseFromString(rsp_env.payload())) {
                std::cerr << "parse OneChatRsp failed\n";
                continue;
            }

            std::cout << "OneChatRsp errcode=" << rsp.errcode() << " msg_id=" << rsp.msg_id()
                      << " errmsg=" << rsp.errmsg() << '\n';
            netutil::drainNotifications(fd);
            continue;
        }

        if (choice == 7) {
            std::string name;
            std::string old_pw;
            std::string new_pw;
            readLine("username: ", name);
            readLine("old password: ", old_pw);
            readLine("new password: ", new_pw);

            chat::ResetPasswordReq req;
            req.set_username(name);
            req.set_old_password(old_pw);
            req.set_new_password(new_pw);

            chat::ChatEnvelope env;
            env.set_msgid(chat::RESET_PASSWORD_MSG);
            env.set_payload(req.SerializeAsString());

            chat::ChatEnvelope rsp_env;
            if (!exchange(fd, env, rsp_env)) {
                break;
            }

            chat::ResetPasswordRsp rsp;
            if (!rsp.ParseFromString(rsp_env.payload())) {
                std::cerr << "parse ResetPasswordRsp failed\n";
                continue;
            }

            std::cout << "ResetPasswordRsp errcode=" << rsp.errcode()
                      << " errmsg=" << rsp.errmsg() << '\n';
            continue;
        }

        if (choice == 8) {
            std::string name;
            std::string password;
            readLine("username: ", name);
            readLine("password: ", password);

            chat::SwitchAccountReq req;
            req.set_username(name);
            req.set_password(password);

            chat::ChatEnvelope env;
            env.set_msgid(chat::SWITCH_ACCOUNT_MSG);
            env.set_payload(req.SerializeAsString());

            chat::ChatEnvelope rsp_env;
            if (!exchange(fd, env, rsp_env)) {
                break;
            }

            chat::SwitchAccountRsp rsp;
            if (!rsp.ParseFromString(rsp_env.payload())) {
                std::cerr << "parse SwitchAccountRsp failed\n";
                continue;
            }

            std::cout << "SwitchAccountRsp errcode=" << rsp.errcode() << " uid=" << rsp.uid()
                      << " name=" << rsp.name() << " errmsg=" << rsp.errmsg() << '\n';

            if (rsp.errcode() == 0) {
                logged_in_uid = rsp.uid();
                netutil::drainNotifications(fd);
            }
            continue;
        }

        if (choice == 9) {
            if (logged_in_uid == 0) {
                std::cout << "please login or register first\n";
                continue;
            }

            std::string peer_str;
            std::string limit_str;
            std::string before_str;
            readLine("friend uid: ", peer_str);
            readLine("limit (empty=default 20): ", limit_str);
            readLine("before_msg_id (empty=latest page): ", before_str);

            int peer_uid = 0;
            try {
                peer_uid = std::stoi(peer_str);
            } catch (...) {
                std::cout << "invalid uid\n";
                continue;
            }

            chat::FetchHistoryReq req;
            req.set_peer_uid(peer_uid);
            try {
                if (!limit_str.empty()) {
                    req.set_limit(std::stoi(limit_str));
                }
                if (!before_str.empty()) {
                    req.set_before_msg_id(std::stoll(before_str));
                }
            } catch (...) {
                std::cout << "invalid limit or before_msg_id\n";
                continue;
            }

            chat::ChatEnvelope env;
            env.set_msgid(chat::FETCH_HISTORY_MSG);
            env.set_payload(req.SerializeAsString());

            chat::ChatEnvelope rsp_env;
            if (!exchange(fd, env, rsp_env)) {
                break;
            }

            chat::FetchHistoryRsp rsp;
            if (!rsp.ParseFromString(rsp_env.payload())) {
                std::cerr << "parse FetchHistoryRsp failed\n";
                continue;
            }

            std::cout << "FetchHistoryRsp errcode=" << rsp.errcode()
                      << " count=" << rsp.messages_size() << " errmsg=" << rsp.errmsg()
                      << '\n';
            for (const auto& entry : rsp.messages()) {
                std::cout << "  [" << entry.msg_id() << "] from=" << entry.from_uid()
                          << " to=" << entry.to_uid() << " \"" << entry.content() << "\"\n";
            }
            continue;
        }

        std::cout << "unknown option\n";
    }

    ::close(fd);
    return 0;
}
