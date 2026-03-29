# 偏移与结构参考

可靠度优先级：**KenshiLib > RE_Kenshi > CT 表**（KenshiLib 结构最稳定）

## RVA 来源

| 功能 | 1.0.64 RVA | 1.0.68 RVA | 来源 | 说明 |
|------|------------|------------|------|------|
| record_status | 0x6CED66 | 0x884AED | CT | 按 C 打开属性面板时 rcx=Character；无面板时用 GameWorld+0x0888 回退 |
| lock_hp | 0x6CEA99 | 0x88AA69 | CT | movss xmm0,[rsi+70] 读取 HP |
| money | 0x5b04f5 | 0x5b04f5 | CT | mov edx,[rax+88] 金钱读取 |
| main_loop | 0x7877A0 | 0x7877A0 | KenshiLib | GameWorld::mainLoop_GPUSensitiveStuff，捕获 rcx=GameWorld* |
| get_time_stamp | 0x25B040 | 0x25B040 | KenshiLib | GameWorld::getTimeStamp 返回游戏小时数 |
| time_update | 0x214B50 | 0x214B50 | RE | TimeUpdate(timeManager, deltaTime)，捕获 rcx=TimeManager* |

## 结构偏移（Character 基址，KenshiLib）

| 偏移 | 用途 | 来源 |
|------|------|------|
| +0x10 | 金钱链起点 | CT/KenshiLib Ownerships |
| +0x18 | 姓名 char+0x18->+0x10 | CT |
| +0x20..+0x28 | 位置 Ogre::Vector3 | KenshiLib CharBody |
| +0x70 | HP | CT lock_hp |
| +0x658 | platoon | KenshiLib Character::platoon |

## 小队结构（KenshiLib）

- Character::platoon 0x658 -> Platoon*
- Platoon::activePlatoon 0x1D8 -> ActivePlatoon*
- ActivePlatoon::squadleader 0xA0 -> Character*

## TimeManager 结构（TimeUpdate 第一参数）

- `+0x08`：timeOfDay (float, 0.0–1.0)
- `+0x10`：gameSpeed (float)

## KenshiLib 对应

- Character::getPosition RVA 0x5CDF00
- Character::getMoney RVA 0x790400
- Character::teleport RVA 0x5C7D60 / 0x5C9F00
- CharBody::getPosition RVA 0x620910
- CharBody::getName RVA 0x639930
- GameWorld::getTimeStamp RVA 0x25B040

## 版本检测

RE_Kenshi：读取 `currentVersion.txt` 解析版本号。
