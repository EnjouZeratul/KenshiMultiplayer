# Kenshi 偏移扫描器 使用说明

## 概述

`offset_scanner.py` 用于在**游戏运行时**或**离线**状态下，通过字节模式扫描获取 CT 表与联机模块所需的基址、RVA、偏移。

**安全承诺：所有操作均为只读，不修改游戏文件或进程内存。**

- 不注入 DLL
- 不写入游戏内存
- 不修改 exe 或任何游戏文件
- 进程模式下仅使用 `ReadProcessMemory` 读取

---

## 运行环境

- Python 3.7+
- Windows（使用 ctypes 调用 Windows API）
- 无额外依赖（仅标准库）

---

## 使用方式

### 1. 自动模式（推荐）

```bash
python offset_scanner.py
```

- 若检测到 `kenshi_x64.exe` 进程 → 使用**进程模式**（读取运行中游戏内存）
- 若未检测到进程 → 使用**文件模式**（读取 exe 文件）

### 2. 强制文件模式

```bash
python offset_scanner.py --file "C:\Program Files (x86)\Steam\steamapps\common\Kenshi\kenshi_x64.exe"
```

适用于：游戏未运行，或希望离线分析 exe。

### 3. 强制进程模式

```bash
python offset_scanner.py --process
```

自动查找 Kenshi 进程并附加。

```bash
python offset_scanner.py --process 12345
```

指定进程 PID 为 12345。

### 4. 输出到 JSON

```bash
python offset_scanner.py --output offsets.json
```

将扫描结果保存到 `offsets.json`，便于后续更新 CT 表或 `game_types.cpp`。

### 5. 自检（无需 exe）

```bash
python offset_scanner.py --test
```

验证模式解析与匹配逻辑，无需游戏文件或进程。用于确认脚本工作正常。

---

## 输出示例

```
=== Kenshi 偏移扫描结果 ===

模式: process
进程 PID: 12345

找到的 RVA:
  lock_hp: 0x6CEA99  (movss xmm0,[rsi+70] 读取 HP，rsi=角色)
  record_status_94: 0x6CED66  (movss xmm6,[rcx+94] 1.0.64 CharStats)
  money: 0x5B04F5  (mov edx,[rax+88] 金钱读取)

已保存到: offsets.json
```

---

## 扫描的模式

| 名称 | 字节模式 | 说明 |
|------|----------|------|
| lock_hp | F3 0F 10 46 70 | 鼠标左键选取角色时读取 HP，rsi=角色 |
| record_status_94 | F3 0F 10 B1 94 00 00 00 | 按 C 打开属性面板，1.0.64 |
| record_status_cc | F3 0F 10 B1 CC 00 00 00 | 按 C 打开属性面板，1.0.68 |
| record_status_wild | F3 0F 10 B1 ?? ?? ?? ?? | 任意版本 record_status |
| money | 8B 90 88 00 00 00 | 金钱读取 |

---

## 根据扫描结果修复 CT 表

1. 运行扫描并保存结果：
   ```bash
   python offset_scanner.py --output offsets.json
   ```

2. 打开 `offsets.json`，查看各 RVA。

3. 若 `lock_hp` 的 RVA 与 CT 表中 `6CEA99` 不同，说明游戏版本可能变化，需更新 CT 表：
   - 打开 `kenshi1.0.64.CT`（或对应版本 CT）
   - 找到 `define(address_lockHP,"kenshi_x64.exe"+6CEA99)`
   - 将 `6CEA99` 替换为 `offsets.json` 中的 `lock_hp` RVA（去掉 `0x` 前缀，转小写）

4. 同理可更新 `record_status`、`money` 等地址。

---

## 进程模式权限说明

进程模式需要读取目标进程内存。若提示「无法读取进程内存」：

1. **以管理员身份运行**命令提示符或 PowerShell，再执行脚本
2. 或关闭杀毒软件对「读取其他进程内存」的拦截（部分安全软件会阻止）

文件模式无需特殊权限。

---

## 与 KenshiMultiplayer 联机模块的关系

- 联机模块的 `game_types.cpp` 中 `GetOffsetsForVersion()` 使用硬编码 RVA
- 若游戏更新导致偏移变化，可先用本工具扫描得到新 RVA
- 将新 RVA 填入 `game_types.cpp` 对应版本分支
- 详见 [OFFSETS.md](../OFFSETS.md)

---

## 扩展：添加新扫描模式

在 `offset_scanner.py` 的 `PATTERNS` 字典中添加：

```python
"新功能名": {
    "bytes": "48 8B 41 ?? 48 85 C0",  # 使用 ?? 表示通配字节
    "desc": "功能描述",
    "mask": None,
},
```

保存后重新运行即可。
