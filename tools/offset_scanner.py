#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Kenshi 偏移扫描器 / Offset Scanner

在游戏运行时或离线模式下，通过字节模式扫描获取 CT 表与联机模块所需的基址、RVA、偏移。
所有操作均为只读，不修改游戏文件或进程内存。

模式：
  1. 文件模式：直接读取 kenshi_x64.exe 文件（离线，100% 安全）
  2. 进程模式：附加到运行中的 Kenshi 进程，只读内存（不注入、不写入）

用法：
  python offset_scanner.py                    # 自动检测：有进程则进程模式，否则文件模式
  python offset_scanner.py --file <exe路径>   # 强制文件模式
  python offset_scanner.py --process [PID]    # 强制进程模式，可选指定 PID
  python offset_scanner.py --output offsets.json  # 指定输出文件
"""

import argparse
import ctypes
import json
import os
import struct
import sys
from ctypes import wintypes
from pathlib import Path
from typing import List, Optional, Tuple

# Windows API
kernel32 = ctypes.windll.kernel32
PROCESS_VM_READ = 0x0010
PROCESS_QUERY_INFORMATION = 0x0400
MEM_COMMIT = 0x1000
PAGE_READONLY = 0x02
PAGE_READWRITE = 0x04
PAGE_EXECUTE_READ = 0x20
PAGE_EXECUTE_READWRITE = 0x40

# CT 表与联机模块关心的模式
PATTERNS = {
    "lock_hp": {
        "bytes": "F3 0F 10 46 70",
        "desc": "movss xmm0,[rsi+70] 读取 HP，rsi=角色",
        "mask": None,
    },
    "record_status_94": {
        "bytes": "F3 0F 10 B1 94 00 00 00",
        "desc": "movss xmm6,[rcx+94] 1.0.64 CharStats",
        "mask": None,
    },
    "record_status_cc": {
        "bytes": "F3 0F 10 B1 CC 00 00 00",
        "desc": "movss xmm6,[rcx+CC] 1.0.68 CharStats",
        "mask": None,
    },
    "record_status_wild": {
        "bytes": "F3 0F 10 B1 ?? ?? ?? ??",
        "desc": "movss xmm6,[rcx+disp32] 任意偏移",
        "mask": "xxx?????",  # 前3字节固定，后4字节通配
    },
    "money": {
        "bytes": "8B 90 88 00 00 00",
        "desc": "mov edx,[rax+88] 金钱读取",
        "mask": None,
    },
}

# 默认 exe 路径（按优先级）
def _default_exe_paths() -> List[Path]:
    """返回要尝试的 exe 路径列表。优先使用 KenshiMultiplayer 上一层目录。"""
    paths = []
    # 1. KenshiMultiplayer 上一层目录（相对路径）
    script_dir = Path(__file__).resolve().parent
    parent_of_kenshi_mp = script_dir.parent.parent  # tools/ -> KenshiMultiplayer/ -> 上一层
    paths.append(parent_of_kenshi_mp / "kenshi_x64.exe")
    # 2. 常见 Steam 路径
    paths.extend([
        Path(r"C:\Program Files (x86)\Steam\steamapps\common\Kenshi\kenshi_x64.exe"),
        Path(r"D:\SteamLibrary\steamapps\common\Kenshi\kenshi_x64.exe"),
    ])
    paths.append(Path("kenshi_x64.exe"))
    return paths


def parse_pattern(pattern_str: str) -> Tuple[bytes, bytes]:
    """解析 'F3 0F 10 46 70' 或 'F3 0F 10 ?? ??' 格式，返回 (byte_mask, check_mask)。"""
    parts = pattern_str.upper().replace(",", " ").split()
    byte_mask = []
    check_mask = []
    for p in parts:
        if p in ("?", "??"):
            byte_mask.append(0)
            check_mask.append(ord("?"))
        else:
            byte_mask.append(int(p, 16))
            check_mask.append(ord("x"))
    return bytes(byte_mask), bytes(check_mask)


def find_pattern_in_buffer(data: bytes, byte_mask: bytes, check_mask: bytes, instance: int = 0) -> Optional[int]:
    """在 data 中查找模式，返回偏移；instance 为第几个匹配。"""
    n = len(check_mask)
    if n == 0 or len(data) < n:
        return None
    idx = 0
    while idx + n <= len(data):
        match = True
        for i in range(n):
            if check_mask[i] == ord("?"):
                continue
            if data[idx + i] != byte_mask[i]:
                match = False
                break
        if match:
            if instance <= 0:
                return idx
            instance -= 1
        idx += 1
    return None


def get_exe_path_from_args_or_default(args) -> Optional[Path]:
    """从参数或默认路径获取 exe。"""
    if getattr(args, "file", None):
        p = Path(args.file)
        if p.exists():
            return p
        return None
    for p in _default_exe_paths():
        if p.exists():
            return p
    return None


def find_kenshi_process() -> Optional[int]:
    """查找 kenshi_x64.exe 进程 PID。"""
    try:
        import ctypes.wintypes
        from ctypes import byref, sizeof

        TH32CS_SNAPPROCESS = 0x00000002
        INVALID_HANDLE_VALUE = -1

        class PROCESSENTRY32(ctypes.Structure):
            _fields_ = [
                ("dwSize", wintypes.DWORD),
                ("cntUsage", wintypes.DWORD),
                ("th32ProcessID", wintypes.DWORD),
                ("th32DefaultHeapID", ctypes.POINTER(ctypes.c_ulong)),
                ("th32ModuleID", wintypes.DWORD),
                ("cntThreads", wintypes.DWORD),
                ("th32ParentProcessID", wintypes.DWORD),
                ("pcPriClassBase", wintypes.LONG),
                ("dwFlags", wintypes.DWORD),
                ("szExeFile", wintypes.WCHAR * 260),
            ]

        CreateToolhelp32Snapshot = kernel32.CreateToolhelp32Snapshot
        Process32First = kernel32.Process32FirstW
        Process32Next = kernel32.Process32NextW
        CloseHandle = kernel32.CloseHandle

        h = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)
        if h == INVALID_HANDLE_VALUE:
            return None

        pe = PROCESSENTRY32()
        pe.dwSize = sizeof(PROCESSENTRY32)
        if Process32First(h, byref(pe)):
            while True:
                if "kenshi_x64.exe" in pe.szExeFile:
                    CloseHandle(h)
                    return pe.th32ProcessID
                if not Process32Next(h, byref(pe)):
                    break
        CloseHandle(h)
    except Exception:
        pass
    return None


def get_module_base(pid: int) -> Optional[Tuple[int, int]]:
    """获取进程内 kenshi_x64.exe 的基址和大小。返回 (base, size) 或 None。"""
    try:
        from ctypes import byref, sizeof

        TH32CS_SNAPMODULE = 0x00000008
        TH32CS_SNAPMODULE32 = 0x00000010
        INVALID_HANDLE_VALUE = -1

        class MODULEENTRY32W(ctypes.Structure):
            _fields_ = [
                ("dwSize", wintypes.DWORD),
                ("th32ModuleID", wintypes.DWORD),
                ("th32ProcessID", wintypes.DWORD),
                ("GlsblcntUsage", wintypes.DWORD),
                ("ProccntUsage", wintypes.DWORD),
                ("modBaseAddr", ctypes.c_void_p),
                ("modBaseSize", wintypes.DWORD),
                ("hModule", ctypes.c_void_p),
                ("szModule", wintypes.WCHAR * 256),
                ("szExePath", wintypes.WCHAR * 260),
            ]

        CreateToolhelp32Snapshot = kernel32.CreateToolhelp32Snapshot
        Module32First = kernel32.Module32FirstW
        Module32Next = kernel32.Module32NextW
        OpenProcess = kernel32.OpenProcess
        CloseHandle = kernel32.CloseHandle

        h_snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid)
        if h_snap == INVALID_HANDLE_VALUE:
            return None

        me = MODULEENTRY32W()
        me.dwSize = sizeof(MODULEENTRY32W)
        if Module32First(h_snap, byref(me)):
            while True:
                if "kenshi_x64.exe" in me.szModule:
                    base = ctypes.addressof(me.modBaseAddr.contents) if hasattr(me.modBaseAddr, "contents") else 0
                    base_ptr = me.modBaseAddr
                    if base_ptr is None:
                        CloseHandle(h_snap)
                        return None
                    try:
                        base = base_ptr.value if base_ptr else 0
                    except Exception:
                        base = ctypes.cast(ctypes.byref(me.modBaseAddr), ctypes.POINTER(ctypes.c_ulonglong))[0]
                    size = me.modBaseSize
                    CloseHandle(h_snap)
                    return (base, size)
                if not Module32Next(h_snap, byref(me)):
                    break
        CloseHandle(h_snap)
    except Exception:
        pass
    return None


def read_process_memory(pid: int, base: int, size: int) -> Optional[bytes]:
    """从进程读取内存。"""
    try:
        OpenProcess = kernel32.OpenProcess
        ReadProcessMemory = kernel32.ReadProcessMemory
        CloseHandle = kernel32.CloseHandle

        h = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, False, pid)
        if not h:
            return None
        buf = (ctypes.c_byte * size)()
        nread = ctypes.c_size_t()
        ok = ReadProcessMemory(h, base, buf, size, ctypes.byref(nread))
        CloseHandle(h)
        if ok:
            return bytes(buf)
    except Exception:
        pass
    return None


def get_text_section_from_pe(data: bytes) -> Optional[Tuple[int, int, int]]:
    """从 PE 获取 .text 段：返回 (raw_offset, raw_size, rva)。"""
    if len(data) < 0x40 or data[:2] != b"MZ":
        return None
    e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
    if e_lfanew + 4 > len(data) or data[e_lfanew : e_lfanew + 4] != b"PE\x00\x00":
        return None
    coff = e_lfanew + 4
    num_sections = struct.unpack_from("<H", data, coff + 2)[0]
    opt_size = struct.unpack_from("<H", data, coff + 16)[0]
    sec_offset = coff + 20 + opt_size
    for i in range(num_sections):
        off = sec_offset + i * 40
        if off + 40 > len(data):
            break
        name = data[off : off + 8].rstrip(b"\x00")
        if name == b".text":
            vsize = struct.unpack_from("<I", data, off + 8)[0]
            rva = struct.unpack_from("<I", data, off + 12)[0]
            raw_size = struct.unpack_from("<I", data, off + 16)[0]
            raw_offset = struct.unpack_from("<I", data, off + 20)[0]
            return (raw_offset, raw_size, rva)
    return None


def scan_file_mode(exe_path: Path) -> dict:
    """文件模式：从磁盘读取 exe 并扫描。"""
    with open(exe_path, "rb") as f:
        data = f.read()
    text_info = get_text_section_from_pe(data)
    if not text_info:
        return {"error": "无法解析 PE 或找不到 .text 段", "mode": "file"}
    raw_off, raw_size, rva_base = text_info
    text_data = data[raw_off : raw_off + raw_size]
    results = {"mode": "file", "exe": str(exe_path), "rva_base": rva_base, "found": {}}
    for name, info in PATTERNS.items():
        bm, cm = parse_pattern(info["bytes"])
        pos = find_pattern_in_buffer(text_data, bm, cm)
        if pos is not None:
            rva = rva_base + pos
            results["found"][name] = {
                "rva": f"0x{rva:X}",
                "rva_int": rva,
                "desc": info["desc"],
            }
    return results


def scan_process_mode(pid: Optional[int] = None) -> dict:
    """进程模式：附加到运行中的 Kenshi，只读内存扫描。"""
    if pid is None:
        pid = find_kenshi_process()
    if not pid:
        return {"error": "未找到 kenshi_x64.exe 进程，请先启动游戏", "mode": "process"}
    mod = get_module_base(pid)
    if not mod:
        return {"error": f"无法获取进程 {pid} 的模块信息", "mode": "process"}
    base, size = mod
    # .text 通常在 0x1000 开始
    text_start = base + 0x1000
    text_size = min(size - 0x1000, 0x2000000)  # 最多 32MB
    mem = read_process_memory(pid, text_start, text_size)
    if not mem:
        return {"error": "无法读取进程内存（需管理员权限？）", "mode": "process", "pid": pid}
    rva_base = 0x1000
    results = {"mode": "process", "pid": pid, "base": f"0x{base:X}", "rva_base": rva_base, "found": {}}
    for name, info in PATTERNS.items():
        bm, cm = parse_pattern(info["bytes"])
        pos = find_pattern_in_buffer(mem, bm, cm)
        if pos is not None:
            rva = rva_base + pos
            results["found"][name] = {
                "rva": f"0x{rva:X}",
                "rva_int": rva,
                "desc": info["desc"],
            }
    return results


def run_self_test() -> bool:
    """自检：验证模式解析与匹配逻辑，无需 exe。"""
    # lock_hp: F3 0F 10 46 70
    bm, cm = parse_pattern("F3 0F 10 46 70")
    test_data = bytes.fromhex("F30F1046704889")  # 匹配 + 后续
    pos = find_pattern_in_buffer(test_data, bm, cm)
    if pos != 0:
        print(f"自检失败: lock_hp 应匹配于 0，得到 {pos}")
        return False
    # record_status_wild: F3 0F 10 B1 ?? ?? ?? ??
    bm2, cm2 = parse_pattern("F3 0F 10 B1 ?? ?? ?? ??")
    test_data2 = bytes.fromhex("F30F10B194000000")  # 94 00 00 00
    pos2 = find_pattern_in_buffer(test_data2, bm2, cm2)
    if pos2 != 0:
        print(f"自检失败: record_status_wild 应匹配于 0，得到 {pos2}")
        return False
    print("自检通过: 模式解析与匹配逻辑正常")
    return True


def main():
    parser = argparse.ArgumentParser(description="Kenshi 偏移扫描器 - 只读，不修改游戏")
    parser.add_argument("--file", type=str, help="强制文件模式：exe 路径")
    parser.add_argument("--process", type=int, nargs="?", const=-1, metavar="PID", help="强制进程模式，可选 PID")
    parser.add_argument("--output", "-o", type=str, default="", help="输出 JSON 文件路径")
    parser.add_argument("--test", action="store_true", help="运行自检（无需 exe）")
    args = parser.parse_args()

    if args.test:
        ok = run_self_test()
        sys.exit(0 if ok else 1)

    result = None
    if args.file:
        exe = get_exe_path_from_args_or_default(args)
        if exe:
            result = scan_file_mode(exe)
        else:
            result = {"error": f"文件不存在: {args.file}", "mode": "file"}
    elif args.process is not None:
        pid = None if args.process == -1 else args.process
        result = scan_process_mode(pid)
    else:
        pid = find_kenshi_process()
        if pid:
            result = scan_process_mode(pid)
            print("检测到 Kenshi 进程，使用进程模式（只读内存）")
        else:
            exe = get_exe_path_from_args_or_default(args)
            if exe:
                result = scan_file_mode(exe)
                print("未检测到进程，使用文件模式（读取 exe 文件）")
            else:
                result = {"error": "未找到 kenshi_x64.exe 且未指定 --file 路径", "mode": "auto"}

    if "error" in result:
        print(f"错误: {result['error']}")
        sys.exit(1)

    print("\n=== Kenshi 偏移扫描结果 ===\n")
    print(f"模式: {result['mode']}")
    if "exe" in result:
        print(f"文件: {result['exe']}")
    if "pid" in result:
        print(f"进程 PID: {result['pid']}")
    print("\n找到的 RVA:")
    for name, info in result.get("found", {}).items():
        print(f"  {name}: {info['rva']}  ({info['desc']})")

    if args.output:
        out_path = Path(args.output)
        out_path.parent.mkdir(parents=True, exist_ok=True)
        with open(out_path, "w", encoding="utf-8") as f:
            json.dump(result, f, ensure_ascii=False, indent=2)
        print(f"\n已保存到: {out_path}")

    return result


if __name__ == "__main__":
    main()
