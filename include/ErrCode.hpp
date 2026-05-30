#ifndef ERRCODE_HPP
#define ERRCODE_HPP

// 业务错误码，与 proto 中 int32 errcode 一致

#include <cstdint>

namespace errc {

constexpr int32_t kOk = 0;
constexpr int32_t kUserExists = 1;
constexpr int32_t kUserNotFound = 2;
constexpr int32_t kWrongPassword = 3;
constexpr int32_t kNotLoggedIn = 4;
constexpr int32_t kNotImplemented = 5;
constexpr int32_t kKicked = 6;
constexpr int32_t kInvalidRequest = 7;
constexpr int32_t kInternalError = 8;
constexpr int32_t kNotFriend = 9;
constexpr int32_t kAlreadyFriend = 10;
constexpr int32_t kCannotAddSelf = 11;
constexpr int32_t kInvalidUid = 12;       // uid 不存在（未注册）
constexpr int32_t kAlreadyLoggedIn = 13;  // 已登录会话不可再次注册
constexpr int32_t kGroupNotFound = 14;
constexpr int32_t kNotGroupMember = 15;
constexpr int32_t kAlreadyInGroup = 16;
constexpr int32_t kGroupFull = 17;
constexpr int32_t kUserGroupLimit = 18;
constexpr int32_t kJoinPending = 19;
constexpr int32_t kNotGroupOwner = 20;
constexpr int32_t kJoinNotPending = 21;

}  // namespace errc

#endif
