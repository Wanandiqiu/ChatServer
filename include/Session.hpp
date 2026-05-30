#ifndef SESSION_HPP
#define SESSION_HPP

// v0.2.0 — 单条 TCP 连接
// v0.4.4 — 空闲超时（任意帧或心跳刷新计时器）
// uid_：0 表示未登录；登录成功后为注册表中的唯一用户 uid

#include <boost/asio.hpp>
#include <chrono>
#include <deque>
#include <memory>
#include <string>

#include "chat.pb.h"

using boost::asio::ip::tcp;

class Session : public std::enable_shared_from_this<Session> {
public:
    explicit Session(tcp::socket socket);

    void start();
    void send(const std::string& frame_body);
    void send(const chat::ChatEnvelope& envelope);

    void setUid(int uid) { uid_ = uid; }
    int uid() const { return uid_; }

    void resetAuth() { uid_ = 0; }

    void shutdown();

    static constexpr int kIdleTimeoutSec = 90;
    static constexpr int kRecommendedHeartbeatSec = 30;

private:
    void doRead();
    void doWrite();
    void onFrame(const std::string& frame_body);
    void close();
    void touchActivity();
    void scheduleIdleCheck();

    tcp::socket socket_;
    boost::asio::steady_timer idle_timer_;
    std::chrono::seconds idle_timeout_;
    std::string read_buffer_;
    std::deque<std::string> write_queue_;
    bool writing_{false};
    int uid_{0};

    enum { kReadChunkSize = 4096 };
    char read_chunk_[kReadChunkSize]{};
};

#endif
