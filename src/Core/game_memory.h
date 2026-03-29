#pragma once

#include "memory.h"
#include "game_types.h"
#include <optional>
#include <vector>

namespace KenshiMP {

/// High-level game memory access (uses Memory + GameOffsets)
class GameMemory {
public:
    static void Init();
    static void Shutdown();

    /// Get current game version from process (reads currentVersion.txt or memory)
    static GameVersion GetGameVersion();

    /// Read player money (cached offset)
    static std::optional<int32_t> ReadMoney();

    /// Write player money
    static bool WriteMoney(int32_t value);

    /// Read selected character base (when 'c' stats panel open - from CE script)
    /// Returns pointer to character object if valid
    static uintptr_t GetSelectedCharacterBase();

    /// Read character position (requires valid character ptr)
    static std::optional<CharacterState> ReadCharacter(uintptr_t char_ptr);

    /// Write character position (KenshiLib CharBody/CharMovement +0x20..+0x28)
    static bool WriteCharacterPosition(uintptr_t char_ptr, float x, float y, float z);

    /// Write character HP (KenshiLib/CT char+0x70)
    static bool WriteCharacterHP(uintptr_t char_ptr, float hp);

    /// Read world time (game hours from getTimeStamp)
    static std::optional<float> ReadWorldTime();

    /// Write world time (game hours) - requires TimeUpdate hook to capture TimeManager
    static bool WriteWorldTime(float game_hours);

    /// Enumerate squad members from selected char (KenshiLib Character::platoon 0x658)
    static std::vector<uintptr_t> EnumerateSquadMembers(uintptr_t char_ptr);

    /// Get offsets for current version
    static const GameOffsets& GetOffsets();
};

} // namespace KenshiMP
