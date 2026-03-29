# KenshiMultiplayer 功能完整设计路线图

> 完全外部挂载的 Kenshi 联机方案，不修改游戏本体与 MOD 系统。删除工具文件夹即可恢复纯净。

---

## 一、目标与原则

- **世界状态同步**：NPC、建筑、物品等需与主机一致
- **客户端加入方式**：自选开局 或 导入 .platoon 文件
- **开局与小队**：开局决定初始小队；中途加入需生成独立小队
- **零侵入**：工具文件夹独立，不修改游戏目录内任何文件

---

## 二、关键模块

| 模块 | 路径 | 作用 |
|------|------|------|
| 同步管理 | `src/Core/sync_manager.cpp` | 聚合/广播角色状态，插值，slot 分配 |
| 网络层 | `src/Core/network.cpp` | UDP P2P，主机/客户端连接 |
| 协议 | `src/Core/protocol.cpp` | StateUpdate、ClientStateReport、SlotAssign、InputEvent、MoneyReport、WorldReload |
| 游戏内存 | `src/Core/game_memory.cpp` | 角色读取、位置/HP 写入、世界时间 |
| Hook | `src/Core/game_hooks.cpp` | record_status、main_loop、time_update |
| 主循环 | `src/Core/game_loop.cpp` | 读取 config，启动 SyncManager |

---

## 三、P2P 架构（主机即服务器）

```
[主机 Kenshi]  <--ENet UDP-->  [客户端 Kenshi]
     |                                |
     | 主机运行游戏 + 承担服务器逻辑 |
     | - 世界权威
     | - 广播实体
     | - 处理客户端上报
     |                                |
 KenshiMP_Core.dll              KenshiMP_Core.dll
 (host + server)                (client)
```

- 主机：运行游戏 + 同时作为权威服务器
- 客户端：连接主机，接收世界状态，上报自身状态
- 无需独立 `KenshiMP.Server.exe`，与当前 P2P 模式一致

### 3.1 客户端与主机联机流程

1. **主机**：运行 KenshiMultiplayer.exe → 启动器选择「主机」、端口（默认 27960）→ 启用联机 → 启动 → 进入游戏
2. **客户端**：运行 KenshiMultiplayer.exe → 启动器选择「客户端」、输入主机 IP:端口 → 启用联机 → 启动 → 进入游戏
3. **连接**：客户端发送 KMP_CONN（含 mod_hash），主机校验 MOD 一致后接受，分配 slot
4. **同步**：主机聚合己方+各客户端角色状态广播 StateUpdate；客户端上报 ClientStateReport，接收 SlotAssign

### 3.2 当前实现范围

- 主机与客户端各自操控选中小队（recordStatus Hook 或 GameWorld 角色列表回退，无需开 C 面板）
- 主机 slot 0，客户端 slot 1、2… 由主机分配
- 主机将客户端角色写入己方小队成员 1、2… 以便看到对方；客户端 `client_apply_position: true` 时同理

---

## 四、世界状态同步

### 4.1 同步范围（设计目标）

| 类型 | 说明 |
|------|------|
| 角色/NPC | 位置、HP、阵营、模板 |
| 建筑/门 | 建造、开关、归属 |
| 物品/背包 | 装备、库存 |
| 阵营 | 关系、派系 |
| 时间/天气 | 世界时间、天气 |

### 4.2 Zone 兴趣管理（规划）

- 将世界划分为 Zone（如 512×512 单位）
- 客户端只接收当前 Zone 及邻域内的实体

### 4.3 生成管道（Spawn Pipeline）

- **不直接调用** `SpawnCharacterDirect`（易崩溃）
- **就地重放**：当主机走到城镇/NPC 附近，游戏自然触发 `CharacterCreate`
- Hook `CharacterCreate`，在回调中把"远程玩家角色"替换进本次创建
- 保证 faction 等指针有效，避免 use-after-free

---

## 五、客户端加入方式

### 5.1 开局（Start Scenario）

**开局的作用**：选择角色背景故事、大致属性、装备、阵营关系；支持捏脸等自定义。**每个玩家可自由选择**，无需与主机一致。

- 开局决定：初始角色数量、类型、出生点、与 NPC 阵营的初始关系
- 客户端加入时：自由选择开局，生成自己的小队
- 当前实现：客户端点 NEW GAME 后自选开局，无协议同步；主机与客户端世界由服务器快照统一

### 5.2 方式 B：导入 .platoon 文件

**流程：**

1. 客户端选择「导入 platoon 加入」
2. 选择本地 `.platoon` 文件（Kenshi 导出的小队）
3. 解析 platoon 数据，通过 `Platoon::loadFromSerialise` 或 `loadStateData` 加载
4. 主机分配 slot，客户端将导入的小队注册为"自己的小队"
5. 主机将该小队同步给其他玩家

**KenshiLib 相关接口：**

- `Platoon::loadFromSerialise(GameSaveState*)`
- `Platoon::loadStateData(GameData*)`
- `Platoon::setDataFilename(string)` — 可能用于 .platoon 路径
- `PlayerInterface::createSquad()` — 创建新小队槽
- `RootObjectFactory::createRandomUnloadedCharacter` — 从 UnloadedPlatoon 创建角色

**难点：**

- .platoon 文件格式需逆向
- 需在正确时机（游戏已加载、世界已存在）调用加载接口

### 5.3 中途加入与小队

**开局决定小队：**

- 开局决定初始角色数量、类型、出生点
- 每个玩家自由选择开局 → 各自生成不同角色

**中途加入：**

- 不能简单"选一个已有角色"：该角色属于已有玩家
- 必须为中途加入者**生成新小队**：
  - 方案 1：主机指定出生点，客户端用同一开局生成新小队
  - 方案 2：客户端导入 platoon，主机在指定位置生成对应实体
  - 方案 3：主机直接生成"远程玩家小队"：调用 `RootObjectFactory::createRandomUnloadedCharacter` 等，传入 UnloadedPlatoon 或模板

---

## 六、实现阶段建议

### 阶段 1：基础（已完成）

- [x] 主机/客户端连接
- [x] 房主 IP 显示
- [x] 角色位置/HP 同步
- [x] 各自操控各自角色（player_slot）

### 阶段 2：世界状态同步

1. **Entity 注册与解析**
   - 引入 `EntityRegistry`、`EntityResolver`
   - 网络 EntityID ↔ 游戏对象指针

2. **Zone 兴趣**
   - 按玩家位置划分 Zone
   - 主机只广播兴趣范围内的实体
   - 客户端请求/订阅 Zone

3. **NPC/角色同步**
   - Hook `CharacterCreate`（就地重放方案）
   - 同步位置、HP、阵营
   - 战斗同步（combat_hooks）

4. **建筑/门**
   - building_hooks
   - 建造、拆除、开关

5. **物品/背包**
   - inventory_hooks
   - 装备、库存变更

### 阶段 3：加入流程

1. **开局**
   - 客户端自由选择开局（背景、属性、装备、捏脸）
   - 无需与主机开局一致

2. **Platoon 导入**
   - 解析 .platoon 格式
   - 调用 `loadFromSerialise` / `loadStateData`
   - 主机在指定位置生成该小队

3. **中途加入**
   - 主机分配出生点
   - 为新玩家生成独立小队（开局或 platoon）

### 阶段 4：体验与稳定性

- [x] **断线重连小队恢复**：90s 无数据超时断开（避免短暂卡顿误踢）；按 IP 暂存 slot 与角色状态；同 IP 重连恢复原 slot
- [x] **存档/读档同步**：WorldReload 协议已实现；主机通过世界时间回退检测自动广播；客户端收到后清空快照
- [ ] **主机下线后继续游戏**：存档保存 ownerName；读档后按玩家名匹配恢复（需游戏存档格式支持）
- [x] 崩溃防护：SEH 异常处理 + minidump 写入 `logs/kenshi_mp_crash.dmp`

---

## 七、.platoon 文件

- 位置：`saves/<save_name>/platoon/` 或类似
- 格式：需逆向，可能为 GameData/GameSaveState 序列化
- KenshiLib：`Platoon::serialise` → `GameSaveState`，`loadFromSerialise` 反向

---

## 七.1 阵营关系与好感分离（规划）

### Kenshi 原生结构

- **FactionRelations**：每个 Faction 有 `RelationData`，存储该阵营对其他阵营的关系值（-100～+100）
- **Platoon::Ownerships**：每个小队有 `Faction* faction`（Platoon.h 0x70），即玩家小队的阵营
- **结论**：每个玩家小队 = 独立 Faction → 阵营关系**天然按玩家小队分离**（不同玩家小队 = 不同阵营 = 各自的关系表）

### 规划实现

- Hook `FactionRelation(factionA, factionB, relation)`，客户端变更时发送，服务器广播
- 关系存储在游戏原生结构中，按 factionId 对 (A,B) 存储

### 是否足以支持分离？

- **是**：Kenshi 原生已按 Faction 存储关系；每个玩家小队有独立 Faction，关系自然分离
- **注意**：若多玩家共享同一 Faction（如同队），则关系会共享；当前设计下每个玩家小队独立，即每个玩家小队对应独立 Faction

---

## 八、外观同步（Request Struct 传输）— 规划

- **协议扩展**：EntitySpawn 在 templateName 后增加可选 requestStruct 字段
- **发送**：玩家角色创建时附带完整 request struct（约 1KB）
- **接收**：就地重放时优先使用 requestStruct 而非 templateName 查找

---

## 九、断线重连小队恢复 — 已实现（含持久化）

- **超时检测**：主机 90s 未收到客户端数据则断开（避免短暂卡顿误踢）
- **匹配依据**：按连接 IP 识别同一玩家
- **服务端**：掉线时将 `m_client_states`、`m_peer_to_slot` 移入 `m_disconnected_squads[IP]`
- **重连**：同 IP 再次连接时恢复原 slot 与角色状态，不递增 `m_next_slot`
- **持久化**：写入 `saves/mp_reconnect.dat`；客户端掉线或主机关机时保存
- **玩家名称**：客户端在启动器输入名称，主机按名称分配 slot；IP 变化时仍可恢复（名称优先，空名称回退到 IP）

---

## 十、存档/读档同步（WorldReload）— 已实现

- **协议**：`PacketType::WorldReload`，主机广播
- **客户端**：收到后清空 `m_snapshot_buffer`，等待新 StateUpdate
- **主机**：`HostTick` 中检测世界时间回退（>2h 且非日循环）自动调用 `BroadcastWorldReload()`

---

## 十一、开发工具

| 工具 | 路径 | 说明 |
|------|------|------|
| 偏移扫描器 | `tools/offset_scanner.py` | 运行时/离线扫描 CT 表与联机模块所需 RVA，纯只读，不修改游戏。详见 `tools/OFFSET_SCANNER_README.md` |

---

## 十二、参考项目文件索引

| 功能 | 路径 |
|------|------|
| Zone | `reference/online/KenshiMP.Core/sync/zone_engine.cpp`, `zone_interest.cpp` |
| Entity | `reference/online/KenshiMP.Core/sync/entity_registry.cpp`, `entity_resolver.cpp` |
| Spawn | `reference/online/KenshiMP.Core/game/spawn_manager.cpp` |
| Entity Hook | `reference/online/KenshiMP.Core/hooks/entity_hooks.cpp` |
| 协议 | `reference/online/KenshiMP.Common/include/kmp/messages.h` |
| 管道 | `reference/online/KenshiMP.Core/sync/pipeline_orchestrator.cpp` |

*文档版本：2025-03，随实现更新*
