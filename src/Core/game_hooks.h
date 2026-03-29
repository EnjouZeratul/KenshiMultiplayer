#pragma once

#include <cstdint>

namespace KenshiMP {

struct GameOffsets;

/// Install hooks for capturing game state
bool InstallGameHooks(const GameOffsets& offsets);

void UninstallGameHooks();

/// Get last captured character (when stats panel was opened with C)
uintptr_t GetSelectedCharacterBaseFromHook();

/// Get GameWorld* (captured from main loop, KenshiLib)
uintptr_t GetGameWorldFromHook();

/// Get TimeManager* (captured from TimeUpdate hook, for writing world time)
uintptr_t GetTimeManagerFromHook();

} // namespace KenshiMP
