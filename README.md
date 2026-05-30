# ChatServer（Boost.Asio + Protobuf）

学习型 IM 后端：TCP 长度前缀帧 + `ChatEnvelope` + SQLite 账号体系。

**版本与功能清单：** [docs/VERSIONS.md](docs/VERSIONS.md)（当前 **v0.4.4**）

## 依赖（macOS Homebrew）

```bash
brew install boost protobuf@3 sqlite libsodium
```

## 构建

```bash
mkdir -p build && cd build
cmake ..
cmake --build .
```

产物：`bin/chat`（服务端）、`bin/chat_cli`（交互客户端）、`bin/proto_client`（冒烟测试）。

## 运行

终端 1：

```bash
./bin/chat
```

终端 2：

```bash
./bin/chat_cli
```

菜单：1 注册 → 2 登录 → 3 登出 → 4 退出 → 5 加好友 → 6 单聊。用户数据在 `data/chat.db`。

## 协议要点

- 帧：`[4 字节 body 长度][protobuf ChatEnvelope]`
- 已实现：`REG_*`、`LOGIN_*`、`LOGINOUT_*`、`ADD_FRIEND_*`、`ONE_CHAT_MSG`
- 同账号重复登录会挤掉旧连接（`errcode=6`）

## 版本规划

详见 [docs/VERSIONS.md](docs/VERSIONS.md)。当前 **v0.4.2** 含账号、好友、单聊、历史与群聊；后续计划 Web 接入。
