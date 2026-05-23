//
// Created by Cheems on 2026/5/24.
//

#include <boost/asio.hpp>
#include <iostream>

using boost::asio::ip::tcp;

// 异步会话处理函数
void session(tcp::socket sock) {
    try {
        for (;;) {
            char data[1024];
            boost::system::error_code error;
            // 非阻塞式读取
            size_t length = sock.read_some(boost::asio::buffer(data), error);

            if (error == boost::asio::error::eof) {
                break;
            }
            else if (error) {
                throw boost::system::system_error(error);
            }

            boost::asio::write(sock, boost::asio::buffer("Hello World!\n"));
        }
    }catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
}

void server_start(boost::asio::io_context& io_context, unsigned short port) {
    tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), port));

    for (;;) {
        // 等待并接受一个新的连接，然后交给 session 函数处理
        std::thread(session, acceptor.accept()).detach();
    }
}

int main() {
    try {
        boost::asio::io_context io_context;
        server_start(io_context, 12345);
    }catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    return 0;
}