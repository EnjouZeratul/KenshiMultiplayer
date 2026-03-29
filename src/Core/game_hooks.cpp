#include "game_hooks.h"
#include "game_types.h"
#include "memory.h"
#include "logger.h"
#include <Windows.h>
#include <cstring>

namespace KenshiMP {

static volatile uintptr_t s_selected_char = 0;
static volatile uintptr_t s_game_world = 0;
static volatile uintptr_t s_time_manager = 0;
static void* s_trampoline = nullptr;
static void* s_hook_code = nullptr;
static void* s_gw_trampoline = nullptr;
static void* s_gw_hook_code = nullptr;
static void* s_time_trampoline = nullptr;
static void* s_time_hook_code = nullptr;
static uint8_t s_original_bytes[16] = {};
static size_t s_patch_size = 0;

uintptr_t GetSelectedCharacterBaseFromHook() {
    return s_selected_char;
}

uintptr_t GetGameWorldFromHook() {
    return s_game_world;
}

uintptr_t GetTimeManagerFromHook() {
    return s_time_manager;
}

static bool SafeReadMemory(void* dest, const void* src, size_t n) {
    __try {
        memcpy(dest, src, n);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static bool InstallRecordStatusHook(uintptr_t base, uintptr_t rva) {
    void* target = reinterpret_cast<void*>(base + rva);
    if (!SafeReadMemory(s_original_bytes, target, 16)) {
        Logger::Error("Failed to read recordStatus bytes");
        return false;
    }

    // Allocate trampoline: original 14 bytes (we patch 14) + jmp back
    s_trampoline = VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!s_trampoline) return false;

    uint8_t* tramp = static_cast<uint8_t*>(s_trampoline);
    memcpy(tramp, s_original_bytes, 14);
    tramp += 14;
    tramp[0] = 0xFF; tramp[1] = 0x25; tramp[2] = 0; tramp[3] = 0; tramp[4] = 0; tramp[5] = 0;
    *reinterpret_cast<void**>(tramp + 6) = static_cast<uint8_t*>(target) + 14;

    // Allocate hook code
    s_hook_code = VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!s_hook_code) {
        VirtualFree(s_trampoline, 0, MEM_RELEASE);
        return false;
    }

    uint8_t* hook = static_cast<uint8_t*>(s_hook_code);
    // mov [s_selected_char], rcx  - 48 89 0D [rel32]
    hook[0] = 0x48; hook[1] = 0x89; hook[2] = 0x0D;
    int32_t disp = static_cast<int32_t>(reinterpret_cast<uintptr_t>(&s_selected_char) - (reinterpret_cast<uintptr_t>(hook) + 7));
    *reinterpret_cast<int32_t*>(hook + 3) = disp;
    // jmp trampoline (trampoline runs full original 14 bytes + jmp back)
    hook[7] = 0xFF; hook[8] = 0x25; hook[9] = 0; hook[10] = 0; hook[11] = 0; hook[12] = 0;
    *reinterpret_cast<void**>(hook + 13) = tramp;

    // Patch target: jmp hook (14 bytes for absolute jmp)
    s_patch_size = 14;
    DWORD old_protect;
    if (!VirtualProtect(target, s_patch_size, PAGE_EXECUTE_READWRITE, &old_protect)) {
        VirtualFree(s_hook_code, 0, MEM_RELEASE);
        VirtualFree(s_trampoline, 0, MEM_RELEASE);
        return false;
    }
    uint8_t* patch = static_cast<uint8_t*>(target);
    patch[0] = 0xFF; patch[1] = 0x25; patch[2] = 0; patch[3] = 0; patch[4] = 0; patch[5] = 0;
    *reinterpret_cast<void**>(patch + 6) = s_hook_code;
    VirtualProtect(target, s_patch_size, old_protect, &old_protect);

    Logger::Info("recordStatus hook installed (GameWorld fallback when C panel closed)");
    return true;
}

static bool InstallGameWorldHook(uintptr_t base, uintptr_t rva) {
    void* target = reinterpret_cast<void*>(base + rva);
    uint8_t orig[16] = {};
    if (!SafeReadMemory(orig, target, 16)) {
        Logger::Error("Failed to read mainLoop bytes");
        return false;
    }
    s_gw_trampoline = VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!s_gw_trampoline) return false;
    uint8_t* tramp = static_cast<uint8_t*>(s_gw_trampoline);
    memcpy(tramp, orig, 14);
    tramp += 14;
    tramp[0] = 0xFF; tramp[1] = 0x25; tramp[2] = 0; tramp[3] = 0; tramp[4] = 0; tramp[5] = 0;
    *reinterpret_cast<void**>(tramp + 6) = static_cast<uint8_t*>(target) + 14;

    s_gw_hook_code = VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!s_gw_hook_code) {
        VirtualFree(s_gw_trampoline, 0, MEM_RELEASE);
        return false;
    }
    uint8_t* hook = static_cast<uint8_t*>(s_gw_hook_code);
    hook[0] = 0x48; hook[1] = 0x89; hook[2] = 0x0D;
    int32_t disp = static_cast<int32_t>(reinterpret_cast<uintptr_t>(&s_game_world) - (reinterpret_cast<uintptr_t>(hook) + 7));
    *reinterpret_cast<int32_t*>(hook + 3) = disp;
    hook[7] = 0xFF; hook[8] = 0x25; hook[9] = 0; hook[10] = 0; hook[11] = 0; hook[12] = 0;
    *reinterpret_cast<void**>(hook + 13) = tramp;

    DWORD old_protect;
    if (!VirtualProtect(target, 14, PAGE_EXECUTE_READWRITE, &old_protect)) {
        VirtualFree(s_gw_hook_code, 0, MEM_RELEASE);
        VirtualFree(s_gw_trampoline, 0, MEM_RELEASE);
        return false;
    }
    uint8_t* patch = static_cast<uint8_t*>(target);
    patch[0] = 0xFF; patch[1] = 0x25; patch[2] = 0; patch[3] = 0; patch[4] = 0; patch[5] = 0;
    *reinterpret_cast<void**>(patch + 6) = s_gw_hook_code;
    VirtualProtect(target, 14, old_protect, &old_protect);
    Logger::Info("GameWorld hook installed - world time available");
    return true;
}

static bool InstallTimeUpdateHook(uintptr_t base, uintptr_t rva) {
    if (rva == 0) return true;  // optional
    void* target = reinterpret_cast<void*>(base + rva);
    uint8_t orig[16] = {};
    if (!SafeReadMemory(orig, target, 16)) {
        Logger::Error("Failed to read TimeUpdate bytes");
        return false;
    }
    s_time_trampoline = VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!s_time_trampoline) return false;
    uint8_t* tramp = static_cast<uint8_t*>(s_time_trampoline);
    memcpy(tramp, orig, 14);
    tramp += 14;
    tramp[0] = 0xFF; tramp[1] = 0x25; tramp[2] = 0; tramp[3] = 0; tramp[4] = 0; tramp[5] = 0;
    *reinterpret_cast<void**>(tramp + 6) = static_cast<uint8_t*>(target) + 14;

    s_time_hook_code = VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!s_time_hook_code) {
        VirtualFree(s_time_trampoline, 0, MEM_RELEASE);
        return false;
    }
    uint8_t* hook = static_cast<uint8_t*>(s_time_hook_code);
    hook[0] = 0x48; hook[1] = 0x89; hook[2] = 0x0D;
    int32_t disp = static_cast<int32_t>(reinterpret_cast<uintptr_t>(&s_time_manager) - (reinterpret_cast<uintptr_t>(hook) + 7));
    *reinterpret_cast<int32_t*>(hook + 3) = disp;
    hook[7] = 0xFF; hook[8] = 0x25; hook[9] = 0; hook[10] = 0; hook[11] = 0; hook[12] = 0;
    *reinterpret_cast<void**>(hook + 13) = tramp;

    DWORD old_protect;
    if (!VirtualProtect(target, 14, PAGE_EXECUTE_READWRITE, &old_protect)) {
        VirtualFree(s_time_hook_code, 0, MEM_RELEASE);
        VirtualFree(s_time_trampoline, 0, MEM_RELEASE);
        return false;
    }
    uint8_t* patch = static_cast<uint8_t*>(target);
    patch[0] = 0xFF; patch[1] = 0x25; patch[2] = 0; patch[3] = 0; patch[4] = 0; patch[5] = 0;
    *reinterpret_cast<void**>(patch + 6) = s_time_hook_code;
    VirtualProtect(target, 14, old_protect, &old_protect);
    Logger::Info("TimeUpdate hook installed - world time write available");
    return true;
}

bool InstallGameHooks(const GameOffsets& offsets) {
    uintptr_t base = Memory::GetModuleBase();
    if (base == 0) return false;
    if (!InstallRecordStatusHook(base, offsets.record_status)) return false;
    InstallGameWorldHook(base, offsets.main_loop);
    InstallTimeUpdateHook(base, offsets.time_update);
    return true;
}

void UninstallGameHooks() {
    if (s_trampoline) { VirtualFree(s_trampoline, 0, MEM_RELEASE); s_trampoline = nullptr; }
    if (s_hook_code) { VirtualFree(s_hook_code, 0, MEM_RELEASE); s_hook_code = nullptr; }
    if (s_gw_trampoline) { VirtualFree(s_gw_trampoline, 0, MEM_RELEASE); s_gw_trampoline = nullptr; }
    if (s_gw_hook_code) { VirtualFree(s_gw_hook_code, 0, MEM_RELEASE); s_gw_hook_code = nullptr; }
    if (s_time_trampoline) { VirtualFree(s_time_trampoline, 0, MEM_RELEASE); s_time_trampoline = nullptr; }
    if (s_time_hook_code) { VirtualFree(s_time_hook_code, 0, MEM_RELEASE); s_time_hook_code = nullptr; }
    s_selected_char = 0;
    s_game_world = 0;
    s_time_manager = 0;
}

} // namespace KenshiMP
