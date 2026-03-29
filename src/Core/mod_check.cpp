#include "mod_check.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <Windows.h>

namespace KenshiMP {

std::vector<ModEntry> ReadGameModList(const std::string& game_path) {
    std::vector<ModEntry> result;
    auto path = std::filesystem::path(game_path) / "data" / "mods.cfg";
    if (!std::filesystem::exists(path)) return result;

    std::ifstream f(path);
    if (!f) return result;

    std::string line;
    int order = 0;
    while (std::getline(f, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
            line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        result.push_back({line, order++});
    }
    return result;
}

std::string ComputeModListHash(const std::vector<ModEntry>& mods) {
    std::stringstream ss;
    for (const auto& m : mods) {
        ss << m.load_order << ":" << m.name << ";";
    }
    return ss.str();
}

bool ModListsMatch(const std::vector<ModEntry>& a, const std::vector<ModEntry>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i].name != b[i].name || a[i].load_order != b[i].load_order)
            return false;
    }
    return true;
}

uint64_t ComputeModListHash64(const std::vector<ModEntry>& mods) {
    std::string str = ComputeModListHash(mods);
    uint64_t hash = 0;
    for (char c : str) {
        hash = hash * 31 + static_cast<unsigned char>(c);
    }
    return hash;
}

std::string GetGameDirectory() {
    wchar_t path[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, path, MAX_PATH) == 0) return {};
    std::filesystem::path p(path);
    return p.parent_path().string();
}

bool IsModListCheckEnabled() {
    wchar_t path[MAX_PATH] = {};
    HMODULE hSelf = nullptr;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&ReadGameModList), &hSelf) && hSelf) {
        GetModuleFileNameW(hSelf, path, MAX_PATH);
    }
    std::filesystem::path p(path);
    auto config_path = p.parent_path() / "config" / "mod_compat.json";
    if (!std::filesystem::exists(config_path)) return true;

    std::ifstream f(config_path);
    if (!f) return true;

    std::stringstream buf;
    buf << f.rdbuf();
    std::string content = buf.str();

    if (content.find("\"mod_list_check\": false") != std::string::npos) return false;
    if (content.find("\"mod_list_check\":false") != std::string::npos) return false;
    return true;
}

} // namespace KenshiMP
