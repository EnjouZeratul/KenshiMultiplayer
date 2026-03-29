#include "game_types.h"

namespace KenshiMP {

// Reliability: KenshiLib > RE_Kenshi > CT (KenshiLib highest - version-agnostic structures)

GameOffsets GetOffsetsForVersion(const GameVersion& ver) {
    // 1.0.64 - KenshiLib GameWorld RVA; CT for record_status etc
    if (ver.major == 1 && ver.minor == 0 && ver.patch == 64) {
        return {
            0x6CED66,  // record_status [CT]
            0x6CEA99,  // lock_hp [CT]
            0x5b04f5,  // money [CT]
            0x6CF9B2,  // game_speed [CT]
            0x54A41A,  // build_no_material [CT]
            0x55978A,  // fast_build [CT]
            0x650A71,  // cut_limb [CT]
            0x6a6130,  // pay_crime [CT]
            0x554D20,  // relation [CT]
            0x71143A,  // item_num [CT]
            0x7877A0,  // main_loop [KenshiLib] GameWorld::mainLoop_GPUSensitiveStuff
            0x25B040,  // get_time_stamp [KenshiLib] GameWorld::getTimeStamp
            0x214B50,  // time_update - TimeUpdate(timeManager, deltaTime)
        };
    }
    if (ver.major == 1 && ver.minor == 0 && ver.patch == 65) {
        return GetOffsetsForVersion({1, 0, 64});
    }
    // 1.0.68 - main_loop/get_time_stamp may differ
    if (ver.major == 1 && ver.minor == 0 && ver.patch == 68) {
        return {
            0x884AED, 0x88AA69, 0x5b04f5, 0x886852,
            0x54A41A, 0x55978A, 0x650A71, 0x853780, 0x6B2D60, 0x71143A,
            0x7877A0, 0x25B040, 0x214B50,  // time_update
        };
    }
    return GetOffsetsForVersion({1, 0, 64});
}

} // namespace KenshiMP
