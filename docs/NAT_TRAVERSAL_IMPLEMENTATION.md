# NAT 穿透实现状态

## 实现概述

已实现完整的 NAT 穿透功能，支持互联网玩家连接到主机，同时保持局域网联机功能。

## 已完成的模块

### 1. UPnP 管理器 (`src/Core/upnp_manager.h`, `src/Core/upnp_manager.cpp`)

- 使用 miniupnp 库实现 UPnP 端口映射
- 自动发现路由器并添加端口转发规则
- 获取外部公网 IP 地址

**类接口：**
```cpp
class UPnPManager {
public:
    bool Initialize();                              // 初始化 UPnP
    void Shutdown();                                // 释放资源
    bool AddPortMapping(...);                       // 添加端口映射
    bool DeletePortMapping(...);                    // 删除端口映射
    std::string GetExternalIPAddress() const;       // 获取公网 IP
    bool IsAvailable() const;                       // 检查 UPnP 是否可用
};
```

### 2. STUN 管理器 (`src/Core/stun_manager.h`, `src/Core/stun_manager.cpp`)

- 基于 RFC 5389 的轻量级 STUN 客户端
- 支持多个冗余 STUN 服务器
- NAT 类型检测

**内置 STUN 服务器：**
- stun.l.google.com:19302
- stun1.l.google.com:19302
- stun.stunprotocol.org:3478
- 223.5.5.5:3478 (阿里云)
- 180.76.76.76:3478 (百度)

### 3. NAT 穿透管理器 (`src/Core/nat_traversal.h`, `src/Core/nat_traversal.cpp`)

- 统一管理 UPnP 和 STUN
- 实现优先级降级：UPnP → STUN → 局域网模式
- 提供连接地址信息（公网 IP 和本地 IP）

**穿透结果枚举：**
```cpp
enum class NatTraversalResult {
    SuccessUPnP,        // UPnP 成功
    SuccessSTUN,        // STUN 成功
    SuccessLocalOnly,   // 仅局域网
    UpnpFailed,         // UPnP 失败
    StunFailed,         // STUN 失败
    Unknown             // 未知状态
};
```

### 4. 本地 IP 工具 (`src/Core/local_ip.h`, `src/Core/local_ip.cpp`)

- 获取本地 IPv4 地址列表
- 优先返回局域网 IP（192.168.x.x、10.x.x.x）

### 5. 网络层集成 (`src/Core/network.h`, `src/Core/network.cpp`)

**新增接口：**
```cpp
// 初始化 NAT 穿透
bool InitializeNAT(bool enable_upnp = true, bool enable_stun = true);

// 获取 NAT 状态
NatStatus GetNATStatus() const;

// 获取连接地址
ConnectionAddresses GetConnectionAddresses() const;

// 获取状态消息
std::string GetNATStatusMessage() const;

// 检查是否支持互联网连接
bool IsInternetAccessible() const;
```

## 配置选项

在 `config/settings.json` 中添加：

```json
{
  "nat_traversal_enabled": true,
  "nat_upnp_enabled": true,
  "nat_stun_enabled": true,
  "custom_stun_server": ""  // 可选自定义 STUN 服务器
}
```

## 连接场景

### 场景 1: UPnP 成功（最佳）
- 主机自动配置端口转发
- 互联网和局域网玩家都可以连接
- 显示公网 IP 和本地 IP

### 场景 2: STUN 成功（次优）
- 通过 STUN 发现公网地址
- 需要手动配置防火墙
- 显示公网 IP 和本地 IP

### 场景 3: 仅局域网（保底）
- UPnP 和 STUN 都失败
- 仅局域网内可连接
- 显示本地 IP

## 使用流程

### 主机
1. 运行 `KenshiMultiplayer.exe`
2. 选择「作为主机」
3. 设置端口（默认 27960）
4. 系统自动执行 NAT 穿透
5. 查看显示的 IP 信息
6. 将 IP 和端口告知要加入的玩家

### 客户端
1. 运行 `KenshiMultiplayer.exe`
2. 选择「加入游戏」
3. 输入主机提供的 IP 地址
4. 输入端口
5. 点击连接

## 故障排除

| 问题 | 可能原因 | 解决方法 |
|------|----------|----------|
| UPnP 失败 | 路由器不支持 UPnP | 在路由器设置中启用 UPnP 或使用 STUN |
| STUN 失败 | 网络连接问题 | 检查网络，更换 STUN 服务器 |
| 仅局域网 | NAT 类型不支持 | 使用虚拟局域网工具（如 ZeroTier） |

## 安全性

- 不修改任何游戏文件
- 纯外部 DLL 注入
- 删除 KenshiMultiplayer 文件夹即可完全卸载
- 所有网络通信使用 UDP 协议

## 后续优化

1. **ICE 框架** - 实现更完整的连接建立流程
2. **打洞技术** - 改善对称 NAT 支持
3. **中继服务器** - 作为最后备选方案
4. **加密通信** - 增加安全性
