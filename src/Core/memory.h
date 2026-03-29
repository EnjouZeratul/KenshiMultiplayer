#pragma once

#include <cstdint>
#include <cstddef>
#include <optional>
#include <string>
#include <Windows.h>

namespace KenshiMP {

/// Memory read/write utilities for game process (called from within injected DLL)
class Memory {
public:
    static void Init();
    static void Shutdown();

    /// Get base address of kenshi_x64.exe
    static uintptr_t GetModuleBase();

    /// Get game directory (parent of kenshi_x64.exe)
    static std::string GetGameDirectory();

    /// Safe read from address
    template<typename T>
    static std::optional<T> Read(uintptr_t address) {
        if (address == 0) return std::nullopt;
        __try {
            return *reinterpret_cast<T*>(address);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return std::nullopt;
        }
    }

    /// Safe write to address
    template<typename T>
    static bool Write(uintptr_t address, const T& value) {
        if (address == 0) return false;
        __try {
            *reinterpret_cast<T*>(address) = value;
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    /// Resolve pointer chain: base + off1 -> + off2 -> + off3
    static uintptr_t ResolvePointer(uintptr_t base, const uintptr_t* offsets, size_t count);
};

} // namespace KenshiMP
