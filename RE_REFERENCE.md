# 逆向参考

> 用于补足偏移、RVA、模式与实现思路。保持简洁版设计，仅取所需。

## 模式与 RVA（v1.0.68 参考）

| 功能 | RVA | 模式（IDA 风格） | 字符串锚点 |
|------|-----|------------------|------------|
| TimeUpdate | 0x214B50 | `40 55 56 48 83 EC 28 48 8B F2 48 8B E9 BA 02...` | timeScale |
| CharacterSetPosition | 0x145E50 | `48 8B C4 55 57 41 54 48 8D 68 C8...` | HavokCharacter::setPosition moved someone off the world |
| GameFrameUpdate | 0x123A10 | `48 8B C4 55 41 54 41 55 41 56 41 57...` | Kenshi 1.0. |
| getTimeStamp | 0x25B040 | - | KenshiLib GameWorld::getTimeStamp |
| main_loop | 0x7877A0 | - | KenshiLib GameWorld::mainLoop_GPUSensitiveStuff |

## TimeManager 结构（TimeUpdate 第一参数）

- `+0x08`：timeOfDay (float, 0.0–1.0)
- `+0x10`：gameSpeed (float)

## 指针链（v1.0.68，base 为 kenshi_x64.exe）

| 名称 | Base RVA | 链偏移 | 说明 |
|------|----------|--------|------|
| PlayerBase | 0x01AC8A90 | - | 玩家数据基址 |
| Money | 0x01AC8A90 | 0x298→0x78→0x88 | 金钱（int） |
| Health | 0x01AC8A90 | 0x2B8→0x5F8→0x40 | HP (float) |
| CharList | 0x01AC8A90 | 0→+8 每下一个 | 角色列表 |

> 本工程使用 Character 基址 + 链，不依赖 PlayerBase。

## 字符串锚点（模式失败时备用）

```
[RootObjectFactory::process] Character
[RootObjectFactory::createRandomSquad] Missing squad leader
HavokCharacter::setPosition moved someone off the world
pathfind
timeScale
Kenshi 1.0.
quicksave
[SaveManager::loadGame] No towns loaded.
```

## 实现要点（可借鉴）

1. **世界时间写入**：Hook TimeUpdate(rcx=timeManager)，捕获 timeManager，写 `timeManager+0x08` 为 timeOfDay。
2. **位置写入**：直接写 CharBody +0x20..+0x28 即可；HavokCharacter::setPosition 需处理 `mov rax,rsp` 序言，trampoline 易出问题。
3. **小队遍历**：Character::platoon 0x658 → Platoon::activePlatoon 0x1D8 → ActivePlatoon::squadleader 0xA0；完整成员需 HandleList::hand 等。
