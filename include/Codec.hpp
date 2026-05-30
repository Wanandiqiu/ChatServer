#ifndef CODEC_HPP
#define CODEC_HPP

// v0.1.0 — TCP 帧编解码
// 帧格式：[4 字节 body 长度，网络字节序][protobuf 序列化 body]

#include <cstdint>
#include <string>

namespace protocol {

constexpr std::size_t kHeaderSize = 4;
constexpr std::uint32_t kMaxBodySize = 64 * 1024;

std::string pack(const std::string& body);

// 从 buffer 尝试拆出一帧；成功时 body 为帧内容并从 buffer 移除
bool try_unpack(std::string& buffer, std::string& body);

}  // namespace protocol

#endif
