/**
 * KenshiMP 协议单元测试
 * 验证序列化/反序列化往返一致性，无需游戏环境
 */
#include "protocol.h"
#include <cstdio>
#include <cstring>

static int g_pass = 0, g_fail = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (cond) { printf("  [PASS] %s\n", msg); g_pass++; } \
    else { printf("  [FAIL] %s\n", msg); g_fail++; } \
} while(0)

int main() {
    printf("KenshiMP Protocol Test\n");
    printf("======================\n");

    // StateUpdate 往返
    {
        KenshiMP::CharacterState chars[2] = {};
        chars[0].pos_x = 1.f; chars[0].pos_y = 2.f; chars[0].pos_z = 3.f;
        chars[0].hp_current = 50.f; chars[0].hp_max = 100.f;
        chars[0].player_slot = 0;
        strncpy(chars[0].name, "Host", sizeof(chars[0].name) - 1);
        chars[1].player_slot = 1;
        strncpy(chars[1].name, "Client1", sizeof(chars[1].name) - 1);

        auto buf = KenshiMP::SerializeStateUpdate(42, 12.5f, chars, 2);
        TEST_ASSERT(!buf.empty(), "SerializeStateUpdate non-empty");

        uint32_t tick;
        float wt;
        KenshiMP::CharacterState out[8];
        uint32_t count = 0;
        bool ok = KenshiMP::DeserializeStateUpdate(buf.data(), buf.size(), tick, wt, out, count, 8);
        TEST_ASSERT(ok, "DeserializeStateUpdate success");
        TEST_ASSERT(tick == 42, "StateUpdate tick");
        TEST_ASSERT(wt == 12.5f, "StateUpdate world_time");
        TEST_ASSERT(count == 2, "StateUpdate count");
        TEST_ASSERT(out[0].player_slot == 0 && out[1].player_slot == 1, "StateUpdate player_slot");
    }

    // ClientStateReport 往返
    {
        KenshiMP::CharacterState c = {};
        c.pos_x = 10.f; c.hp_current = 75.f; c.player_slot = 1;
        auto buf = KenshiMP::SerializeClientStateReport(100, &c, 1);
        TEST_ASSERT(!buf.empty(), "SerializeClientStateReport non-empty");

        uint32_t tick;
        KenshiMP::CharacterState out[4];
        uint32_t count = 0;
        bool ok = KenshiMP::DeserializeClientStateReport(buf.data(), buf.size(), tick, out, count, 4);
        TEST_ASSERT(ok && count == 1 && tick == 100, "ClientStateReport roundtrip");
    }

    // SlotAssign 往返
    {
        auto buf = KenshiMP::SerializeSlotAssign(2);
        TEST_ASSERT(!buf.empty(), "SerializeSlotAssign non-empty");
        auto slot = KenshiMP::DeserializeSlotAssign(buf.data(), buf.size());
        TEST_ASSERT(slot && *slot == 2, "SlotAssign roundtrip");
    }

    // MoneyReport 往返
    {
        KenshiMP::MoneyReportPayload p = { 5000, 200 };
        auto buf = KenshiMP::SerializeMoneyReport(p);
        TEST_ASSERT(!buf.empty(), "SerializeMoneyReport non-empty");
        auto out = KenshiMP::DeserializeMoneyReport(buf.data(), buf.size());
        TEST_ASSERT(out && out->money == 5000 && out->tick == 200, "MoneyReport roundtrip");
    }

    // WorldReload
    {
        auto buf = KenshiMP::SerializeWorldReload();
        TEST_ASSERT(!buf.empty(), "SerializeWorldReload non-empty");
        TEST_ASSERT(KenshiMP::IsWorldReloadPacket(buf.data(), buf.size()), "IsWorldReloadPacket");
    }

    // InputEvent 往返
    {
        KenshiMP::InputEventPayload ev = { 1, 2, 10.f, 20.f, 30.f, 0 };
        auto buf = KenshiMP::SerializeInputEvent(ev);
        TEST_ASSERT(!buf.empty(), "SerializeInputEvent non-empty");
        auto out = KenshiMP::DeserializeInputEvent(buf.data(), buf.size());
        TEST_ASSERT(out && out->pos_x == 10.f, "InputEvent roundtrip");
    }

    printf("======================\n");
    printf("Result: %d pass, %d fail\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
