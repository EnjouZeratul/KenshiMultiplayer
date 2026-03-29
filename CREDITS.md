# 参考与致谢

本项目的实现参考了以下开源项目与社区成果，在此致谢。

---

## 主要参考

### RE_Kenshi

- **项目**：[BFrizzleFoShizzle/RE_Kenshi](https://github.com/BFrizzleFoShizzle/RE_Kenshi)
- **许可**：GPL-3.0
- **用途**：版本检测思路（currentVersion.txt）、Escort 内存/Hook 工具实现参考
- **说明**：社区逆向工程成果，非官方 SDK
- **致谢**：感谢 BFrizzleFoShizzle 及 RE_Kenshi 贡献者，其版本检测与 Hook 实现为本项目提供了参考

### KenshiLib

- **项目**：[KenshiReclaimer/KenshiLib](https://github.com/KenshiReclaimer/KenshiLib)
- **用途**：Character、CharBody、Platoon、GameWorld 等结构；位置、金钱、小队、世界时间 RVA；**偏移可靠度最高**
- **说明**：社区逆向工程成果，非官方 SDK。不同游戏版本需对应不同 KenshiLib 分支/提交
- **致谢**：感谢 KenshiReclaimer 及 KenshiLib 社区，其结构定义与 RVA 是联机实现的核心依据

### Cheat Engine 表

- **用途**：record_status、lock_hp、money、game_speed 等 RVA 与结构偏移的逆向依据
- **说明**：recordStatus 需按 C 打开属性面板后才可捕获角色；金钱依赖选中角色指针链；部分功能依赖特定游戏状态
- **致谢**：感谢未知前辈留下的 CT 表，record_status、金钱指针链等 RVA 与偏移为本项目提供了参考

---

## 版本说明

游戏版本通过 `currentVersion.txt` 自动检测（参考 RE_Kenshi 方案）。当前支持 1.0.64 / 1.0.65 / 1.0.68，未知版本回退至 1.0.64 偏移。
