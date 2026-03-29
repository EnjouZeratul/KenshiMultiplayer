#pragma once

#include <cstdint>
#include <cstddef>

namespace KenshiMP {

/// Game version for offset selection
struct GameVersion {
    int major, minor, patch;
    bool operator==(const GameVersion& o) const {
        return major == o.major && minor == o.minor && patch == o.patch;
    }
};

/// Offsets for kenshi_x64.exe (per version)
/// Reliability: KenshiLib > RE_Kenshi > CT
struct GameOffsets {
    // RVA - CT/RE
    uintptr_t record_status;      // rcx=Character when C stats open
    uintptr_t lock_hp;
    uintptr_t money;
    uintptr_t game_speed;
    uintptr_t build_no_material;
    uintptr_t fast_build;
    uintptr_t cut_limb;
    uintptr_t pay_crime;
    uintptr_t relation;
    uintptr_t item_num;
    // KenshiLib GameWorld::mainLoop_GPUSensitiveStuff - capture rcx=GameWorld*
    uintptr_t main_loop;
    // KenshiLib GameWorld::getTimeStamp
    uintptr_t get_time_stamp;
    // TimeUpdate(timeManager, deltaTime) - capture rcx=TimeManager*, write timeManager+0x08
    uintptr_t time_update;
};

/// Character state for sync
struct CharacterState {
    float pos_x, pos_y, pos_z;
    float rot_y;
    float hp_current, hp_max;
    uint32_t character_id;
    uint32_t squad_id;
    char name[64];
    uint8_t player_slot;  // 0=host, 1=client1, 2=client2... 用于区分各自角色
    uint8_t _pad[3];
};

/// World state for sync
struct WorldState {
    float time_of_day;
    uint32_t weather;
    uint32_t zone_id;
};

/// Squad/party state
struct SquadState {
    uint32_t squad_id;
    uint32_t member_count;
    uint32_t member_ids[32];
};

/// Get offsets for game version (1.0.64 default)
GameOffsets GetOffsetsForVersion(const GameVersion& ver);

} // namespace KenshiMP
