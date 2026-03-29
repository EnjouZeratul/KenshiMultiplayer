#include "memory.h"
#include "logger.h"
#include <Windows.h>
#include <Psapi.h>
#include <filesystem>

namespace KenshiMP {

static uintptr_t s_module_base = 0;

void Memory::Init() {
    s_module_base = reinterpret_cast<uintptr_t>(GetModuleHandleA("kenshi_x64.exe"));
    if (s_module_base) {
        Logger::Info("kenshi_x64.exe base: 0x" + std::to_string(s_module_base));
    } else {
        Logger::Error("Failed to get kenshi_x64.exe base address");
    }
}

std::string Memory::GetGameDirectory() {
    HMODULE hExe = GetModuleHandleA("kenshi_x64.exe");
    if (!hExe) return {};
    wchar_t path[MAX_PATH] = {};
    if (GetModuleFileNameW(hExe, path, MAX_PATH) == 0) return {};
    std::filesystem::path p(path);
    return p.parent_path().string();
}

void Memory::Shutdown() {
    s_module_base = 0;
}

uintptr_t Memory::GetModuleBase() {
    return s_module_base;
}

uintptr_t Memory::ResolvePointer(uintptr_t base, const uintptr_t* offsets, size_t count) {
    uintptr_t addr = base;
    for (size_t i = 0; i < count; ++i) {
        auto val = Read<uintptr_t>(addr);
        if (!val || *val == 0) return 0;
        addr = *val + offsets[i];
    }
    return addr;
}

} // namespace KenshiMP
