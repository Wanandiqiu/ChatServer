// 服务端入口：初始化 SQLite，监听 6000，运行 io_context

#include <iostream>

#include "ChatServer.hpp"
#include "ChatService.hpp"
#include "Session.hpp"

int main() {
    try {
        if (!ChatService::instance().init("data/chat.db")) {
            std::cerr << "failed to init ChatService database\n";
            return 1;
        }

        boost::asio::io_context io_context;
        server s(io_context, 6000);
        io_context.run();
    } catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
    }
    return 0;
}