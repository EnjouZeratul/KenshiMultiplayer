#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace KenshiMP {

/// MOD list entry for compatibility check
struct ModEntry {
    std::string name;
    int load_order;
};

/// Read mod list from game's mods.cfg
std::vector<ModEntry> ReadGameModList(const std::string& game_path);

/// Compute hash/checksum of mod list for compatibility
std::string ComputeModListHash(const std::vector<ModEntry>& mods);

/// Compute 64-bit hash for network transmission
uint64_t ComputeModListHash64(const std::vector<ModEntry>& mods);

/// Check if two mod lists match (same mods, same order)
bool ModListsMatch(const std::vector<ModEntry>& a, const std::vector<ModEntry>& b);

/// Get game directory (where kenshi_x64.exe resides)
std::string GetGameDirectory();

/// Check if mod list validation is enabled (from config/mod_compat.json)
bool IsModListCheckEnabled();

} // namespace KenshiMP
