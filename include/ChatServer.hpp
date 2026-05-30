#ifndef CHATSERVER_HPP
#define CHATSERVER_HPP

// v0.1.0 — Boost.Asio TCP 监听与接入
// 每接受一个连接则创建 Session 并开始读循环

#include <boost/asio.hpp>

using boost::asio::ip::tcp;

class server {
public:
    server(boost::asio::io_context& io_context, unsigned short port);

private:
    void do_accept();

    tcp::acceptor acceptor_;
};

#endif
