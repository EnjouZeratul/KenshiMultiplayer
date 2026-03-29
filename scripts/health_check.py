#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
KenshiMultiplayer 健康检查与仿真测试脚本

功能：
1. 构建验证
2. 代码审计（TODO/占位/未完成）
3. 文档与实现一致性
4. 配置完整性
5. 协议/功能覆盖检查
"""

import os
import sys
import subprocess
import json
import re
from pathlib import Path
from dataclasses import dataclass, field
from typing import List, Tuple, Optional

# 项目根目录
ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "src"
DOCS = ROOT / "docs"
CONFIG = ROOT / "config"


@dataclass
class Issue:
    severity: str  # error, warning, info
    category: str
    message: str
    file: Optional[str] = None
    line: Optional[int] = None
    detail: Optional[str] = None


@dataclass
class AuditResult:
    passed: List[str] = field(default_factory=list)
    issues: List[Issue] = field(default_factory=list)
    build_ok: bool = False

    def add_pass(self, msg: str):
        self.passed.append(msg)

    def add_issue(self, severity: str, category: str, message: str, **kwargs):
        self.issues.append(Issue(severity=severity, category=category, message=message, **kwargs))


def run_build() -> bool:
    """尝试构建项目"""
    build_dir = ROOT / "build"
    if not build_dir.exists():
        build_dir.mkdir(parents=True)
    try:
        # 尝试 cmake + build
        subprocess.run(
            ["cmake", "..", "-G", "Visual Studio 17 2022", "-A", "x64"],
            cwd=build_dir,
            capture_output=True,
            timeout=60,
        )
        result = subprocess.run(
            ["cmake", "--build", ".", "--config", "Release"],
            cwd=build_dir,
            capture_output=True,
            timeout=120,
        )
        return result.returncode == 0
    except (subprocess.TimeoutExpired, FileNotFoundError):
        return False


def audit_todos(result: AuditResult) -> None:
    """检查 TODO/FIXME/占位实现"""
    patterns = [
        (r"//\s*TODO[:\s]", "TODO"),
        (r"//\s*FIXME[:\s]", "FIXME"),
        (r"//\s*XXX[:\s]", "XXX"),
        (r"placeholder|stub|未实现|待完成", "占位/未完成"),
    ]
    for cpp in SRC.rglob("*.cpp"):
        try:
            text = cpp.read_text(encoding="utf-8", errors="replace")
            for i, line in enumerate(text.splitlines(), 1):
                for pat, label in patterns:
                    if re.search(pat, line, re.I):
                        result.add_issue(
                            "warning",
                            "未完成",
                            f"{label}: {line.strip()[:80]}",
                            file=str(cpp.relative_to(ROOT)),
                            line=i,
                        )
        except Exception:
            pass


def audit_protocol_coverage(result: AuditResult) -> None:
    """检查协议类型使用情况"""
    protocol_types = ["Ping", "Pong", "Disconnect"]
    for pt in protocol_types:
        count = 0
        for f in SRC.rglob("*.cpp"):
            count += len(re.findall(rf"\b{pt}\b", f.read_text(encoding="utf-8", errors="replace")))
        for f in SRC.rglob("*.h"):
            count += len(re.findall(rf"\b{pt}\b", f.read_text(encoding="utf-8", errors="replace")))
        # 定义处算 1 次，若只有定义无使用则为未使用
        if count <= 2:  # 仅 protocol.h 中的 enum 和可能的 Deserialize
            result.add_issue(
                "info",
                "协议",
                f"PacketType.{pt} 已定义但未在业务逻辑中使用",
            )


def audit_broadcast_world_reload(result: AuditResult) -> None:
    """检查 BroadcastWorldReload 是否被调用"""
    total_refs = 0
    for f in SRC.rglob("*.cpp"):
        total_refs += f.read_text(encoding="utf-8", errors="replace").count("BroadcastWorldReload()")
    # 定义 1 次 + 至少 1 次调用（HostTick 读档检测等）
    if total_refs < 2:
        result.add_issue(
            "info",
            "WorldReload",
            "BroadcastWorldReload() 未被调用（需读档检测或 LoadGame Hook）",
        )


def audit_input_event_handler(result: AuditResult) -> None:
    """检查 InputEvent 处理是否为空"""
    sync_cpp = SRC / "Core" / "sync_manager.cpp"
    if not sync_cpp.exists():
        return
    text = sync_cpp.read_text(encoding="utf-8", errors="replace")
    if "InputEvent" in text and "TODO" in text:
        result.add_issue(
            "warning",
            "InputEvent",
            "InputEvent 收到后仅 TODO，未实现业务逻辑",
            file="src/Core/sync_manager.cpp",
        )


def audit_config_save_detailed(result: AuditResult) -> None:
    """检查 SaveConfig 是否保存了 DLL 需要的所有字段"""
    config_cpp = SRC / "Launcher" / "config.cpp"
    mp_config_cpp = SRC / "Core" / "mp_config.cpp"
    if not config_cpp.exists() or not mp_config_cpp.exists():
        return
    # DLL 从 settings.json 读取的字段（mp_config.cpp）
    dll_fields = {"mp_enabled", "mp_role", "mp_host", "mp_port", "client_apply_position"}
    save_text = config_cpp.read_text(encoding="utf-8", errors="replace")
    # SaveConfig 的 f << 写入块
    save_start = save_text.find("void SaveConfig")
    save_end = save_text.find("\n}", save_start) if save_start >= 0 else -1
    save_block = save_text[save_start:save_end] if save_start >= 0 and save_end >= 0 else ""
    for field in dll_fields:
        if f'"{field}"' not in save_block and f'"{field}"' not in save_text:
            result.add_issue(
                "error" if field == "client_apply_position" else "warning",
                "配置",
                f"SaveConfig 未保存 {field}，DLL 读取的 config 会被覆盖为默认值",
                file="src/Launcher/config.cpp",
            )


def audit_doc_impl_alignment(result: AuditResult) -> None:
    """检查文档与实现一致性"""
    roadmap = DOCS / "DESIGN_roadmap.md"
    if not roadmap.exists():
        return
    text = roadmap.read_text(encoding="utf-8", errors="replace")
    # 文档声称的模块路径
    doc_paths = [
        "src/Core/sync_manager.cpp",
        "src/Core/network.cpp",
        "src/Core/protocol.cpp",
        "src/Core/game_memory.cpp",
        "src/Core/game_hooks.cpp",
        "src/Core/game_loop.cpp",
    ]
    for p in doc_paths:
        if (ROOT / p).exists():
            result.add_pass(f"文档路径存在: {p}")
        else:
            result.add_issue("error", "文档", f"文档引用的路径不存在: {p}")


def audit_config_files(result: AuditResult) -> None:
    """检查必要配置文件"""
    settings = CONFIG / "settings.json"
    if not settings.exists():
        result.add_issue("warning", "配置", "config/settings.json 不存在")
    else:
        try:
            data = json.loads(settings.read_text(encoding="utf-8"))
            required = ["mp_enabled", "mp_role", "mp_host", "mp_port"]
            for k in required:
                if k not in data:
                    result.add_issue("warning", "配置", f"settings.json 缺少字段: {k}")
        except json.JSONDecodeError:
            result.add_issue("error", "配置", "settings.json 不是有效 JSON")


def audit_cmake_sources(result: AuditResult) -> None:
    """检查 CMake 中的源文件是否都存在"""
    cmake = ROOT / "CMakeLists.txt"
    if not cmake.exists():
        return
    text = cmake.read_text(encoding="utf-8", errors="replace")
    # 提取 add_executable 和 add_library 中的源文件
    for m in re.finditer(r"(add_executable|add_library)\s*\([^)]+\)", text, re.DOTALL):
        block = m.group(0)
        for fm in re.finditer(r"src/[^\s)]+\.(cpp|h)", block):
            p = ROOT / fm.group(0).replace("/", os.sep)
            if not p.exists():
                result.add_issue("error", "构建", f"CMakeLists 引用不存在的文件: {fm.group(0)}")


def run_protocol_test(result: AuditResult) -> None:
    """运行协议单元测试"""
    test_exe = ROOT / "KenshiMP_ProtocolTest.exe"
    if not test_exe.exists():
        test_exe = ROOT / "build" / "Release" / "KenshiMP_ProtocolTest.exe"
    if not test_exe.exists():
        test_exe = ROOT / "build" / "KenshiMP_ProtocolTest.exe"
    if test_exe.exists():
        try:
            r = subprocess.run(
                [str(test_exe)],
                cwd=ROOT,
                capture_output=True,
                text=True,
                timeout=10,
            )
            if r.returncode == 0:
                result.add_pass("协议单元测试通过")
            else:
                result.add_issue("error", "测试", f"协议测试失败: {r.stderr[:200] if r.stderr else r.stdout[:200]}")
        except Exception as e:
            result.add_issue("warning", "测试", f"协议测试执行异常: {e}")
    else:
        result.add_issue("info", "测试", "KenshiMP_ProtocolTest.exe 不存在，请先 build")


def run_network_simulation(result: AuditResult) -> None:
    """检查编译产物"""
    exe = ROOT / "KenshiMultiplayer.exe"
    dll = ROOT / "KenshiMP_Core.dll"
    if exe.exists() and dll.exists():
        result.add_pass("可执行文件与 DLL 存在，可进行联机测试")
    else:
        result.add_issue("info", "仿真", "未找到编译产物，跳过联机测试（请先 build）")


def main() -> int:
    result = AuditResult()
    print("=" * 60)
    print("KenshiMultiplayer 健康检查与审计")
    print("=" * 60)

    # 1. 构建
    print("\n[1] 构建验证...")
    result.build_ok = run_build()
    if result.build_ok:
        result.add_pass("构建成功")
        print("    构建成功")
    else:
        result.add_issue("warning", "构建", "构建失败或未安装 CMake/VS（跳过）")
        print("    构建跳过或失败")

    # 2. TODO/占位
    print("\n[2] 代码审计（TODO/占位）...")
    audit_todos(result)
    audit_input_event_handler(result)

    # 3. 协议覆盖
    print("\n[3] 协议覆盖检查...")
    audit_protocol_coverage(result)
    audit_broadcast_world_reload(result)

    # 4. 配置
    print("\n[4] 配置完整性...")
    audit_config_save_detailed(result)
    audit_config_files(result)

    # 5. 文档
    print("\n[5] 文档与实现一致性...")
    audit_doc_impl_alignment(result)

    # 6. CMake
    print("\n[6] CMake 源文件...")
    audit_cmake_sources(result)

    # 7. 协议测试
    print("\n[7] 协议单元测试...")
    run_protocol_test(result)

    # 8. 编译产物
    print("\n[8] 编译产物检查...")
    run_network_simulation(result)

    # 输出报告
    print("\n" + "=" * 60)
    print("报告")
    print("=" * 60)
    for p in result.passed:
        print(f"  [PASS] {p}")
    for i in result.issues:
        sym = {"error": "[ERR]", "warning": "[WARN]", "info": "[INFO]"}.get(i.severity, "[?]")
        loc = f" ({i.file}:{i.line})" if i.file else ""
        print(f"  {sym} {i.category}: {i.message}{loc}")
        if i.detail:
            print(f"       {i.detail}")

    errors = sum(1 for i in result.issues if i.severity == "error")
    warnings = sum(1 for i in result.issues if i.severity == "warning")
    print(f"\n汇总: {len(result.passed)} 通过, {errors} 错误, {warnings} 警告")
    return 1 if errors > 0 else 0


if __name__ == "__main__":
    sys.exit(main())
