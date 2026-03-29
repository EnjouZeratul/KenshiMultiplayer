#include "game_memory.h"
#include "game_hooks.h"
#include "game_types.h"
#include "logger.h"
#include "memory.h"
#include <Windows.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstdio>
#include <algorithm>
#include <cmath>

namespace KenshiMP {

static GameOffsets s_offsets;
static GameVersion s_version = {1, 0, 64};

void GameMemory::Init() {
    s_version = GetGameVersion();
    s_offsets = GetOffsetsForVersion(s_version);
    Logger::Info("GameMemory initialized for version " +
        std::to_string(s_version.major) + "." +
        std::to_string(s_version.minor) + "." +
        std::to_string(s_version.patch));
    if (s_version.major != 1 || s_version.minor != 0 ||
        (s_version.patch != 64 && s_version.patch != 65 && s_version.patch != 68)) {
        Logger::Info("Unsupported version - using 1.0.64 offsets (may be incorrect)");
    }
    InstallGameHooks(s_offsets);
}

void GameMemory::Shutdown() {
    // Don't uninstall hooks - game may still run, and process exit frees all memory
}

GameVersion GameMemory::GetGameVersion() {
    std::string game_dir = Memory::GetGameDirectory();
    if (game_dir.empty()) return {1, 0, 64};

    std::filesystem::path version_file = std::filesystem::path(game_dir) / "currentVersion.txt";
    if (!std::filesystem::exists(version_file)) return {1, 0, 64};

    std::ifstream f(version_file);
    if (!f) return {1, 0, 64};

    std::string line;
    if (!std::getline(f, line) || line.empty()) return {1, 0, 64};

    // Format: "Kenshi 1.0.64 - x64 (Newland)" or "Kenshi 1.0.68 - x64 (Newland)"
    GameVersion ver = {1, 0, 64};
    size_t pos = line.find("Kenshi ");
    if (pos != std::string::npos) {
        pos += 7;  // skip "Kenshi "
        int major = 0, minor = 0, patch = 0;
        if (std::sscanf(line.c_str() + pos, "%d.%d.%d", &major, &minor, &patch) >= 2) {
            ver = {major, minor, patch};
        }
    }
    return ver;
}

std::optional<int32_t> GameMemory::ReadMoney() {
    uintptr_t char_ptr = GetSelectedCharacterBase();
    if (char_ptr == 0) return std::nullopt;

    // Money: CT 钱 chain char+0x10 -> +0x10 -> +0x80 -> +0x88 (KenshiLib Character::getMoney RVA 0x790400)
    auto p1 = Memory::Read<uintptr_t>(char_ptr + 0x10);
    if (!p1 || *p1 == 0) return std::nullopt;
    auto p2 = Memory::Read<uintptr_t>(*p1 + 0x10);
    if (!p2 || *p2 == 0) return std::nullopt;
    auto p3 = Memory::Read<uintptr_t>(*p2 + 0x80);
    if (!p3 || *p3 == 0) return std::nullopt;
    return Memory::Read<int32_t>(*p3 + 0x88);
}

bool GameMemory::WriteMoney(int32_t value) {
    uintptr_t char_ptr = GetSelectedCharacterBase();
    if (char_ptr == 0) return false;
    // Same chain as ReadMoney (CT): char +0x10 -> +0x10 -> +0x80 -> +0x88
    auto p1 = Memory::Read<uintptr_t>(char_ptr + 0x10);
    if (!p1 || *p1 == 0) return false;
    auto p2 = Memory::Read<uintptr_t>(*p1 + 0x10);
    if (!p2 || *p2 == 0) return false;
    auto p3 = Memory::Read<uintptr_t>(*p2 + 0x80);
    if (!p3 || *p3 == 0) return false;
    return Memory::Write(*p3 + 0x88, value);
}

// GameWorld+0x0888 characterList (KenshiLib verified) - fallback when recordStatus not triggered
static constexpr uintptr_t GAMEWORLD_CHARLIST_OFF = 0x0888;

static uintptr_t GetSelectedCharacterFromGameWorldFallback() {
    uintptr_t gw = GetGameWorldFromHook();
    if (gw == 0) return 0;

    uintptr_t lektorBase = gw + GAMEWORLD_CHARLIST_OFF;
    uintptr_t modBase = Memory::GetModuleBase();
    size_t modSize = (modBase != 0) ? 0x4000000 : 0;
    __try {
        auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(modBase);
        if (dos && dos->e_magic == IMAGE_DOS_SIGNATURE) {
            auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(modBase + dos->e_lfanew);
            if (nt->Signature == IMAGE_NT_SIGNATURE)
                modSize = nt->OptionalHeader.SizeOfImage;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}

    auto isValidHeapPtr = [modBase, modSize](uintptr_t val) -> bool {
        if (val < 0x10000 || val >= 0x00007FFFFFFFFFFF) return false;
        if (modBase != 0 && val >= modBase && val < modBase + modSize) return false;
        return true;
    };

    auto c1 = Memory::Read<int>(lektorBase);
    auto p1 = Memory::Read<uintptr_t>(lektorBase + 0x08);
    int lektorCount = c1.value_or(0);
    uintptr_t arrayPtr = p1.value_or(0);
    if (lektorCount <= 0 || lektorCount > 8000 || !isValidHeapPtr(arrayPtr)) {
        auto p2 = Memory::Read<uintptr_t>(lektorBase);
        auto c2 = Memory::Read<int>(lektorBase + 0x08);
        arrayPtr = p2.value_or(0);
        lektorCount = c2.value_or(0);
        if (lektorCount <= 0 || lektorCount > 8000 || !isValidHeapPtr(arrayPtr))
            return 0;
    }

    for (int i = 0; i < lektorCount && i < 500; ++i) {
        auto cp = Memory::Read<uintptr_t>(arrayPtr + i * sizeof(uintptr_t));
        if (!cp || *cp == 0) continue;
        uintptr_t charPtr = *cp;
        if (!isValidHeapPtr(charPtr)) continue;

        auto platoon = Memory::Read<uintptr_t>(charPtr + 0x658);
        if (!platoon || *platoon == 0) continue;
        auto active = Memory::Read<uintptr_t>(*platoon + 0x1D8);
        if (!active || *active == 0) continue;

        auto px = Memory::Read<float>(charPtr + 0x20);
        auto py = Memory::Read<float>(charPtr + 0x24);
        auto pz = Memory::Read<float>(charPtr + 0x28);
        if (!px || !py || !pz || (*px == 0.f && *py == 0.f && *pz == 0.f)) continue;

        auto faction = Memory::Read<uintptr_t>(charPtr + 0x10);
        if (faction && *faction != 0 && isValidHeapPtr(*faction)) {
            auto fid = Memory::Read<uint32_t>(*faction + 0x08);
            if (fid && *fid > 1000) continue;
        }
        return charPtr;
    }
    return 0;
}

uintptr_t GameMemory::GetSelectedCharacterBase() {
    uintptr_t v = GetSelectedCharacterBaseFromHook();
    if (v != 0) return v;
    return GetSelectedCharacterFromGameWorldFallback();
}

std::optional<CharacterState> GameMemory::ReadCharacter(uintptr_t char_ptr) {
    if (char_ptr == 0) return std::nullopt;

    CharacterState state = {};
    state.character_id = 0;
    state.squad_id = 0;

    // Offsets: KenshiLib > RE_Kenshi > CT
    // Position: KenshiLib CharBody/CharMovement; CT/empiric +0x20..+0x28 (Ogre::Vector3)
    // HP: CT lock_hp movss xmm0,[rsi+70] -> +0x70
    auto hp = Memory::Read<float>(char_ptr + 0x70);
    if (hp) state.hp_current = *hp;

    auto pos = Memory::Read<float>(char_ptr + 0x20);
    if (pos) state.pos_x = *pos;
    auto pos2 = Memory::Read<float>(char_ptr + 0x24);
    if (pos2) state.pos_y = *pos2;
    auto pos3 = Memory::Read<float>(char_ptr + 0x28);
    if (pos3) state.pos_z = *pos3;

    // Name: CT 角色姓名 char+0x18 -> +0x10 (KenshiLib CharBody::getName RVA 0x639930)
    auto p1 = Memory::Read<uintptr_t>(char_ptr + 0x18);
    if (p1 && *p1) {
        auto p2 = Memory::Read<uintptr_t>(*p1 + 0x10);
        if (p2 && *p2) {
            __try {
                const char* src = reinterpret_cast<const char*>(*p2);
                size_t n = 0;
                while (n < sizeof(state.name) - 1 && src[n]) { state.name[n] = src[n]; n++; }
                state.name[n] = '\0';
            } __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
    }

    return state;
}

bool GameMemory::WriteCharacterPosition(uintptr_t char_ptr, float x, float y, float z) {
    if (char_ptr == 0) return false;
    // KenshiLib CharBody position Ogre::Vector3 +0x20..+0x28
    return Memory::Write(char_ptr + 0x20, x) &&
           Memory::Write(char_ptr + 0x24, y) &&
           Memory::Write(char_ptr + 0x28, z);
}

bool GameMemory::WriteCharacterHP(uintptr_t char_ptr, float hp) {
    if (char_ptr == 0) return false;
    // KenshiLib/CT char+0x70 (lock_hp movss xmm0,[rsi+70])
    return Memory::Write(char_ptr + 0x70, hp);
}

std::vector<uintptr_t> GameMemory::EnumerateSquadMembers(uintptr_t char_ptr) {
    std::vector<uintptr_t> out;
    if (char_ptr == 0) return out;
    out.push_back(char_ptr);
    // KenshiLib Character::platoon 0x658, Platoon::activePlatoon 0x1D8, ActivePlatoon::squadleader 0xA0
    auto platoon = Memory::Read<uintptr_t>(char_ptr + 0x658);
    if (!platoon || *platoon == 0) return out;
    auto active = Memory::Read<uintptr_t>(*platoon + 0x1D8);
    if (!active || *active == 0) return out;
    auto leader = Memory::Read<uintptr_t>(*active + 0xA0);
    if (leader && *leader && std::find(out.begin(), out.end(), *leader) == out.end())
        out.push_back(*leader);
    return out;
}

std::optional<float> GameMemory::ReadWorldTime() {
    // KenshiLib GameWorld::getTimeStamp() RVA - returns double (game hours)
    uintptr_t gw = GetGameWorldFromHook();
    if (gw == 0) return std::nullopt;
    uintptr_t base = Memory::GetModuleBase();
    if (base == 0) return std::nullopt;
    using Fn = double (*)(void*);
    Fn getTimeStamp = reinterpret_cast<Fn>(base + s_offsets.get_time_stamp);
    __try {
        double hrs = getTimeStamp(reinterpret_cast<void*>(gw));
        return static_cast<float>(hrs);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return std::nullopt;
    }
}

bool GameMemory::WriteWorldTime(float game_hours) {
    // TimeManager+0x08 = timeOfDay (0.0-1.0); timeOfDay = fmod(gameHours, 24) / 24
    uintptr_t tm = GetTimeManagerFromHook();
    if (tm == 0) return false;
    double tod = std::fmod(static_cast<double>(game_hours), 24.0) / 24.0;
    if (tod < 0.0) tod += 1.0;
    return Memory::Write(tm + 0x08, static_cast<float>(tod));
}

const GameOffsets& GameMemory::GetOffsets() {
    return s_offsets;
}

} // namespace KenshiMP
