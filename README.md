# Kenshi P2P 外部联机模块

完全外部挂载的 Kenshi 联机方案，不修改游戏本体与 MOD 系统。

## 安装

1. 将 `KenshiMultiplayer` 文件夹解压到任意位置
2. 若与 `kenshi_x64.exe` 不在同一目录，编辑 `config/settings.json` 设置 `kenshi_path`
3. 运行 `KenshiMultiplayer.exe` 启动游戏并注入联机模块

## 卸载

删除整个 `KenshiMultiplayer` 文件夹即可，游戏恢复纯净状态。
或运行 `uninstall.bat`（若从游戏目录内安装）。

## 构建

### 前置要求

- CMake 3.16+
- Visual Studio 2019/2022/2026（含 C++ 桌面开发）
- Git（用于下载依赖）

### 依赖项：miniupnp

本项目使用 miniupnp 库实现 UPnP 端口转发。构建前需要下载：

```bash
cd third_party
git clone https://github.com/miniupnp/miniupnp.git
# 或使用预发布版本
# curl -L https://github.com/miniupnp/miniupnp/releases/download/2.3.3/miniupnp-2.3.3.tar.gz | tar xz
# mv miniupnp-2.3.3 miniupnp
```

**推荐**：使用 Git 克隆（可获取最新修复）：
```bash
git clone https://github.com/miniupnp/miniupnp.git third_party/miniupnp
```

### 方式一：使用 build.bat（Windows）

```bash
双击运行 build.bat
```

### 方式二：手动构建

```bash
# 确保已下载 miniupnp 到 third_party/miniupnp
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

输出文件将复制到 KenshiMultiplayer 根目录：
- `KenshiMultiplayer.exe` - 启动器
- `KenshiMP_Core.dll` - 注入 DLL

## 目录结构

```
KenshiMultiplayer/
├── KenshiMultiplayer.exe   # 主启动器
├── KenshiMP_Core.dll        # 联机核心（注入到游戏）
├── config/
│   ├── settings.json
│   └── mod_compat.json
├── reference/              # 参考内容（online、kenshilib）
├── saves/
├── logs/
└── uninstall.bat
```

## MOD 严格对齐

联机时强制主机与客户端 MOD 列表及加载顺序完全一致，不一致则拒绝连接。

- 校验内容：`data/mods.cfg` 中的 MOD 名称与顺序
- 可通过 `config/mod_compat.json` 中 `"mod_list_check": false` 关闭（不推荐）

## 版本

当前版本：0.1.0（阶段 1 - 基础框架 + 角色同步）

游戏版本通过 `currentVersion.txt` 自动检测，支持 1.0.64 / 1.0.65 / 1.0.68。

## 当前实现状态

| 功能 | 状态 | 说明 |
|------|------|------|
| 网络连接 / MOD 校验 | ✓ | 主机/客户端连接，MOD 列表一致校验 |
| 选中角色读取 | ✓ | recordStatus Hook + GameWorld 角色列表回退，无需开 C 面板 |
| 角色位置/HP 同步 | ✓ | 主机广播选中角色状态 |
| 小队枚举 | ✓ | KenshiLib Character::platoon 同步选中+队长 |
| 金钱读取/写入 | ✓ | 依赖选中角色，CT 指针链 |
| 世界时间 | ✓ | 读取 getTimeStamp，客户端写入 TimeManager+0x08 |
| 客户端应用同步 | ✓ | `client_apply_position: true` 时，将主机位置/HP/世界时间写入客户端 |
| 断线重连 | ✓ | 90s 超时；按 IP 持久化到 saves/mp_reconnect.dat；跨日重开服可恢复原 slot |
| WorldReload | ✓ | 主机检测世界时间回退自动广播；客户端清空快照 |
| 崩溃防护 | ✓ | SEH + minidump 写入 logs/ |

## 偏移参考

偏移可靠度：**KenshiLib > RE_Kenshi > CT 表**。详见 [OFFSETS.md](OFFSETS.md)。

## 配置说明

- `mp_role`: `"host"` 或 `"client"`
- `mp_player_name`: 客户端玩家名称，用于 slot 分配；IP 变化时仍可恢复原小队（建议填写）
- `client_apply_position`: `"true"` 时，客户端将收到的主机角色位置写入本地选中角色（用于同步测试）

## 健康检查与测试

运行 `python scripts/health_check.py` 或 `scripts/run_health_check.bat` 进行：

- 构建验证、代码审计（TODO/占位）
- 协议覆盖、配置完整性、文档一致性
- 协议单元测试（需先 build 生成 KenshiMP_ProtocolTest.exe）

详见 [scripts/README.md](scripts/README.md)。

## 参考与致谢

详见 [CREDITS.md](CREDITS.md)。

## 许可证

本项目采用 **AGPL-3.0** 许可证。详见 [LICENSE](LICENSE) 文件。
