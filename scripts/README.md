# KenshiMultiplayer 脚本

## health_check.py

健康检查与审计脚本，用于：

1. **构建验证**：尝试 CMake 构建
2. **代码审计**：TODO/FIXME/占位实现
3. **协议覆盖**：Ping/Pong/Disconnect/InputEvent 等是否被使用
4. **配置完整性**：SaveConfig 是否保存 DLL 所需字段
5. **文档一致性**：DESIGN_roadmap 中的路径是否存在
6. **CMake 源文件**：引用的文件是否存在
7. **协议单元测试**：运行 KenshiMP_ProtocolTest.exe
8. **编译产物**：exe/dll 是否存在

### 运行方式

```bash
python scripts/health_check.py
```

或双击 `scripts/run_health_check.bat`

### 输出说明

- `[PASS]` 通过
- `[WARN]` 警告（如 TODO、未实现）
- `[ERR]` 错误（需修复）
- `[INFO]` 提示信息

## 协议单元测试

`tests/protocol_test.cpp` 验证协议序列化/反序列化往返，无需游戏环境。

构建后生成 `KenshiMP_ProtocolTest.exe`，健康检查脚本会自动运行（若存在）。
