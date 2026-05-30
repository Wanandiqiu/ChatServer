//
// Created by Cheems on 2026/5/24.
//

#include "ChatServer.hpp"
#include "Session.hpp"

#include <memory>

server::server(boost::asio::io_context& io_context, unsigned short port)
    : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
    do_accept();
}

// 递归异步接收连接
void server::do_accept() {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket) {
            if (!ec) {
                std::make_shared<Session>(std::move(socket))->start();
            }
            do_accept();
        });
}
