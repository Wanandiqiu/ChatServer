# 版本与功能记录

本项目采用 **语义化版本**（`vMAJOR.MINOR.PATCH`）。`MAJOR` 在大协议或架构不兼容变更时递增；学习阶段以 `0.x.y` 为主。

**当前最新版本：v0.4.3**

详细需求与实现计划见：

- `docs/brainstorms/2026-05-30-chatserver-boost-account-requirements.md`
- `docs/plans/2026-05-30-001-feat-boost-account-milestone-plan.md`
- `docs/plans/2026-05-30-002-feat-chat-history-fetch-plan.md`
- `docs/brainstorms/2026-05-30-group-chat-messaging-requirements.md`
- `docs/plans/2026-05-30-003-feat-group-chat-plan.md`
- `docs/brainstorms/2026-05-30-im-campus-beta-v1-requirements.md`
- `docs/plans/2026-05-30-004-feat-conversation-list-unread-plan.md`

---

## v0.4.3 — 会话列表与未读（2026-05-30）

校园内测路线 **M2**：统一收件箱（单聊 + 群聊混排），未读与 `delivered` 离线语义解耦。

### 能力

| 类别 | 状态 | 说明 |
|------|------|------|
| 会话列表 | ✅ | `LIST_CONVERSATIONS_MSG`：按最近消息时间降序 |
| 未读计数 | ✅ | 入站推送/离线补发时 `unread_count++`；标已读归零 |
| 标已读 | ✅ | `MARK_CONVERSATION_READ_MSG`（进入会话即已读，内测简化） |
| 列表范围 | ✅ | 仅展示有过消息的会话 |
| 存储 | ✅ | `conversation_state` 表 |
| CLI | ✅ | 菜单 18–19 |

### 协议（新增 MsgType 31–34）

| MsgType | 说明 |
|---------|------|
| `LIST_CONVERSATIONS_MSG` / `_ACK` (31/32) | 收件箱列表 |
| `MARK_CONVERSATION_READ_MSG` / `_ACK` (33/34) | 标已读 |

`SessionType`：`SESSION_PEER=1`，`SESSION_GROUP=2`。

### 验收（双终端）

1. A、B 互加好友 → A 菜单 6 发单聊 → B 菜单 18 见 PEER 会话 `unread=1` → 菜单 19 标已读 → 18 显示 `unread=0`
2. 建群发 2 条群消息 → 成员菜单 18 见群会话 `unread=2` → 19 标已读后为 0
3. 回归：菜单 6/12/9/14 单聊群聊历史仍正常

### 校园内测进度

| 里程碑 | 状态 |
|--------|------|
| M2 收件箱 | ✅ v0.4.3 |
| M1 TLS/心跳 | 待办 |
| M3 Web 客户端 | 待办 |

---

## v0.4.2 — 群聊（2026-05-30）

路线图主题 **v0.4.0 群聊** 的实现发布；在 v0.4.1 单聊历史之后递增 patch。

### 能力

| 类别 | 状态 | 说明 |
|------|------|------|
| 建群 | ✅ | 名称、简介；公开 / 需审批模式 |
| 加群 | ✅ | 凭 `group_id` 直接加入或提交申请 |
| 群主审批 | ✅ | `LIST_JOIN_REQUESTS` + `REVIEW_JOIN_REQUEST` |
| 群消息 | ✅ | 实时 `GroupChatNotify`；离线 per-member 补发 |
| 群历史 | ✅ | `FETCH_GROUP_HISTORY_MSG` 分页 |
| 我的群列表 | ✅ | `LIST_MY_GROUPS_MSG` |
| 退群 | ✅ | `LEAVE_GROUP_MSG` |
| 上限 | ✅ | 单群 200 人；每人 50 群 |
| CLI | ✅ | 菜单 10–17 |

### 协议（新增 MsgType 19–30）

| MsgType | 说明 |
|---------|------|
| `CREATE_GROUP_MSG` / `_ACK` (8/19) | 建群 |
| `ADD_GROUP_MSG` / `_ACK` (9/20) | 加入/申请 |
| `GROUP_CHAT_MSG` (10) | 发消息 / 应答 / 推送 |
| `LIST_MY_GROUPS_MSG` / `_ACK` (21/22) | 我的群 |
| `FETCH_GROUP_HISTORY_MSG` / `_ACK` (23/24) | 群历史 |
| `LEAVE_GROUP_MSG` / `_ACK` (25/26) | 退群 |
| `LIST_JOIN_REQUESTS_MSG` / `_ACK` (27/28) | 待审批列表 |
| `REVIEW_JOIN_REQUEST_MSG` / `_ACK` (29/30) | 批准/拒绝 |

### 错误码（新增 14–21）

| 值 | 常量 | 含义 |
|----|------|------|
| 14 | `kGroupNotFound` | 群不存在 |
| 15 | `kNotGroupMember` | 非成员 |
| 16 | `kAlreadyInGroup` | 已在群内 |
| 17 | `kGroupFull` | 群已满 |
| 18 | `kUserGroupLimit` | 用户加群数达上限 |
| 19 | `kJoinPending` | 已有待审申请 |
| 20 | `kNotGroupOwner` | 非群主 |
| 21 | `kJoinNotPending` | 无待审申请 |

### 验收（双终端）

1. A 菜单 10 建公开群 → 记下 `group_id` → B 菜单 11 加入 → 互发菜单 12，双方见 `[group]` 推送
2. C 建需审批群（模式 2）→ D 菜单 11 申请 → C 菜单 16/17 批准 → D 可发群消息
3. B 离线时 A 发群消息 → B 登录（菜单 2）后收到补发 → 菜单 14 拉历史
4. 成员菜单 15 退群后菜单 12 返回 `errcode=15`

---

## v0.4.1 — 单聊历史拉取（2026-05-30）

### 能力

| 类别 | 状态 | 说明 |
|------|------|------|
| 历史分页 | ✅ | `FETCH_HISTORY_MSG`：与好友的双向单聊记录 |
| 游标翻页 | ✅ | `before_msg_id`（0=最新一页）；响应按 id 升序 |
| limit | ✅ | 默认 20，最大 100 |
| 门禁 | ✅ | 须登录；`peer_uid` 须存在且为好友 |
| CLI | ✅ | 菜单 9 拉取并打印历史 |

### 协议

| MsgType | 说明 |
|---------|------|
| `FETCH_HISTORY_MSG` / `FETCH_HISTORY_MSG_ACK` (17/18) | `FetchHistoryReq` / `FetchHistoryRsp` + `HistoryEntry` |

### 验收（双终端）

1. A、B 注册（自动登录）→ 互加好友 → 互发消息
2. A 菜单 9，输入 B 的 `uid`，应看到全部历史（升序）
3. 可选 `before_msg_id` 翻更早一页，无重复遗漏
4. 对非好友 uid → `errcode=9`；非法 uid → `errcode=12`
5. 离线补发行为不变（拉历史不改 `delivered`）

---

## v0.3.2 — 账号增强（2026-05-30）

### 能力

| 类别 | 状态 | 说明 |
|------|------|------|
| 注册自动登录 | ✅ | 注册成功后 `RegRsp.logged_in=true`，Session 立即绑定 uid |
| 重置密码 | ✅ | `RESET_PASSWORD_MSG`：用户名 + 旧密码 + 新密码 |
| 切换账号 | ✅ | `SWITCH_ACCOUNT_MSG`：同连接切换登录用户 |

### 协议

| MsgType | 说明 |
|---------|------|
| `RESET_PASSWORD_MSG` / `_ACK` | 修改密码 |
| `SWITCH_ACCOUNT_MSG` / `_ACK` | 切换账号 |

### CLI 菜单

- `7` 重置密码
- `8` 切换账号

---

## v0.3.1 — 唯一 uid 与登录门禁（2026-05-30）

### 能力

| 类别 | 状态 | 说明 |
|------|------|------|
| 唯一 uid | ✅ | 注册成功分配 `RegRsp.uid` |
| 协议字段 | ✅ | 用户标识统一为 `uid` |
| 登录门禁 | ✅ | 除注册/登录外须已登录 |
| 目标校验 | ✅ | 单聊 `to_uid` 须已注册 |

### 错误码（新增）

| 值 | 常量 | 含义 |
|----|------|------|
| 12 | `kInvalidUid` | 目标 uid 不存在 |
| 13 | `kAlreadyLoggedIn` | 已登录连接不可再次注册 |

---

## v0.3.0 — 好友与单聊（2026-05-30）

### 能力

| 类别 | 状态 | 说明 |
|------|------|------|
| 加好友 | ✅ | 按用户名添加，双向好友关系 |
| 单聊发送 | ✅ | 仅好友可发；消息入库 |
| 实时推送 | ✅ | 接收方在线时 `ONE_CHAT_MSG` + `OneChatNotify` |
| 离线消息 | ✅ | 未在线时标记未投递；**登录后**拉取并推送 |
| CLI | ✅ | 菜单 5 加好友、6 发消息 |

### 协议

| MsgType | 状态 |
|---------|------|
| `ADD_FRIEND_MSG` / `ADD_FRIEND_MSG_ACK` | ✅ |
| `ONE_CHAT_MSG` | ✅ 请求/应答/推送 |
| 群组相关 | ✅ 见 v0.4.2 |

### 错误码（新增）

| 值 | 常量 | 含义 |
|----|------|------|
| 9 | `kNotFriend` | 非好友不能单聊 |
| 10 | `kAlreadyFriend` | 已是好友 |
| 11 | `kCannotAddSelf` | 不能加自己 |

### 数据表（SQLite）

- `friend` — 双向好友
- `chat_message` — 单聊记录，`delivered` 标记

---

## v0.2.0 — 账号里程碑（2026-05-30）

### 能力

| 类别 | 状态 | 说明 |
|------|------|------|
| 用户注册 | ✅ | 用户名唯一；密码 Argon2id 哈希入库 |
| 用户登录 | ✅ | 用户名 + 密码；成功后绑定 `Session` |
| 用户登出 | ✅ | 清除在线表；**保持 TCP** |
| 在线会话 | ✅ | `uid → Session`；挤下线（`errcode=6`） |
| 持久化 | ✅ | SQLite `data/chat.db` |
| 交互客户端 | ✅ | `bin/chat_cli`、`bin/proto_client` |

### 错误码

| 值 | 常量 | 含义 |
|----|------|------|
| 0–8 | 见 `include/ErrCode.hpp` | 含 `kNotLoggedIn`、`kKicked` 等 |

### 验收

1. 注册 → 登录 → 登出
2. 错误密码登录失败
3. `password_hash` 非明文

---

## v0.1.0 — 网络骨架（2026-05）

| 类别 | 状态 | 说明 |
|------|------|------|
| TCP 定长帧 | ✅ | `include/Codec.hpp` |
| 接收拆包 | ✅ | `Session` + `try_unpack` |
| 发送队列 | ✅ | 写队列避免并发 write |
| 连接接入 | ✅ | `ChatServer` `async_accept` |
| 消息分发 | ✅ | `ChatEnvelope` → `ChatService` |

---

## 路线图（未发布）

| 目标版本 | 主题 | 主要能力 |
|----------|------|----------|
| v0.5.0+ | 离线增强 | 已读回执等 |
| 后续 | Web 接入 | WebSocket 或 HTTP 网关 |

版本发布时在本文件顶部更新 **当前最新版本**，并新增对应章节。
