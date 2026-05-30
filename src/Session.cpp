// Session：读循环累积 buffer，拆帧后交给 ChatService；写路径经队列串行化

#include "Session.hpp"

#include "ChatService.hpp"
#include "Codec.hpp"

#include <iostream>
#include <utility>

Session::Session(tcp::socket socket)
    : socket_(std::move(socket)),
      idle_timer_(socket_.get_executor()),
      idle_timeout_(std::chrono::seconds(kIdleTimeoutSec)) {}

void Session::start() {
    touchActivity();
    doRead();
}

void Session::send(const chat::ChatEnvelope& envelope) {
    send(envelope.SerializeAsString());
}

void Session::send(const std::string& frame_body) {
    auto self = shared_from_this();
    boost::asio::post(socket_.get_executor(),
        [this, self, frame = protocol::pack(frame_body)]() mutable {
            const bool was_empty = write_queue_.empty();
            write_queue_.push_back(std::move(frame));
            if (!writing_ && was_empty) {
                doWrite();
            }
        });
}

// read 积累 read_buffer_ 中，并按帧拆包
void Session::doRead() {
    auto self = shared_from_this();
    socket_.async_read_some(boost::asio::buffer(read_chunk_, kReadChunkSize),
        [this, self](boost::system::error_code ec, std::size_t length) {
            if (ec) {
                if (ec != boost::asio::error::operation_aborted) {
                    ChatService::instance().onDisconnect(self);
                }
                close();
                return;
            }

            read_buffer_.append(read_chunk_, length);

            std::string body;
            while (protocol::try_unpack(read_buffer_, body)) {
                onFrame(body);
            }

            if (read_buffer_.size() > protocol::kMaxBodySize + protocol::kHeaderSize) {
                std::cerr << "read buffer overflow, closing session\n";
                close();
                return;
            }

            doRead();
        });
}

void Session::doWrite() {
    if (write_queue_.empty()) {
        writing_ = false;
        return;
    }

    writing_ = true;
    auto self = shared_from_this();
    boost::asio::async_write(
        socket_, boost::asio::buffer(write_queue_.front()),
        [this, self](boost::system::error_code ec, std::size_t /*length*/) {
            if (ec) {
                close();
                return;
            }

            write_queue_.pop_front();
            if (write_queue_.empty()) {
                writing_ = false;
            } else {
                doWrite();
            }
        });
}

void Session::touchActivity() {
    idle_timer_.cancel();
    scheduleIdleCheck();
}

void Session::scheduleIdleCheck() {
    auto self = shared_from_this();
    idle_timer_.expires_after(idle_timeout_);
    idle_timer_.async_wait([this, self](boost::system::error_code ec) {
        if (ec == boost::asio::error::operation_aborted) {
            return;
        }
        std::cout << "session idle timeout, closing connection\n";
        ChatService::instance().onDisconnect(self);
        close();
    });
}

// 收到完整帧后交给 ChatService::onMessage
void Session::onFrame(const std::string& frame_body) {
    touchActivity();
    ChatService::instance().onMessage(shared_from_this(), frame_body);
}

void Session::shutdown() {
    close();
}

void Session::close() {
    boost::system::error_code ec;
    socket_.close(ec);
}
