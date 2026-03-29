#pragma once

#include "game_types.h"
#include <cstdint>
#include <vector>
#include <optional>

namespace KenshiMP {

enum class PacketType : uint8_t {
    Invalid = 0,
    StateUpdate,      // Host -> Client: 所有玩家状态 [host, client1, client2...]
    ClientStateReport,// Client -> Host: 客户端上报自己的角色状态
    SlotAssign,       // Host -> Client: 分配客户端 slot (1, 2, 3...)
    InputEvent,       // Client -> Host: player input
    MoneyReport,      // Client -> Host: 客户端上报金钱
    WorldReload,      // Host -> Client: 主机读档，客户端清空快照等待新状态
    Ping,
    Pong,
    Disconnect,
};

#pragma pack(push, 1)

struct PacketHeader {
    uint8_t magic[4];   // "KMP\0"
    PacketType type;
    uint32_t seq;
    uint32_t payload_len;
};

struct StateUpdatePayload {
    uint32_t tick;
    float world_time;
    uint32_t character_count;
    // Followed by CharacterState[character_count]
};

struct InputEventPayload {
    uint32_t tick;
    uint8_t event_type;
    float pos_x, pos_y, pos_z;
    uint32_t target_id;
};

/// Client -> Host: 金钱上报（主机不写回客户端，仅用于校验/扣除判定）
struct MoneyReportPayload {
    int32_t money;       // 客户端当前金钱
    uint32_t tick;       // 客户端 tick，便于主机排序
};

#pragma pack(pop)

constexpr size_t MAX_PACKET_SIZE = 4096;
constexpr uint32_t PROTOCOL_MAGIC = 0x00504D4B; // "KMP\0" little-endian

/// Serialize state update
std::vector<uint8_t> SerializeStateUpdate(uint32_t tick, float world_time,
    const CharacterState* chars, uint32_t count);

/// Deserialize state update
bool DeserializeStateUpdate(const uint8_t* data, size_t len,
    uint32_t& tick, float& world_time,
    CharacterState* chars, uint32_t& count, uint32_t max_chars);

/// Serialize input event
std::vector<uint8_t> SerializeInputEvent(const InputEventPayload& ev);

/// Deserialize input event
std::optional<InputEventPayload> DeserializeInputEvent(const uint8_t* data, size_t len);

/// Serialize money report (Client -> Host)
std::vector<uint8_t> SerializeMoneyReport(const MoneyReportPayload& ev);

/// Deserialize money report
std::optional<MoneyReportPayload> DeserializeMoneyReport(const uint8_t* data, size_t len);

/// Serialize client state report (Client -> Host)
std::vector<uint8_t> SerializeClientStateReport(uint32_t tick, const CharacterState* chars, uint32_t count);

/// Deserialize client state report
bool DeserializeClientStateReport(const uint8_t* data, size_t len,
    uint32_t& tick, CharacterState* chars, uint32_t& count, uint32_t max_chars);

/// Serialize slot assign (Host -> Client)
std::vector<uint8_t> SerializeSlotAssign(uint8_t slot);

/// Deserialize slot assign
std::optional<uint8_t> DeserializeSlotAssign(const uint8_t* data, size_t len);

/// Serialize WorldReload (Host -> Client: 主机读档)
std::vector<uint8_t> SerializeWorldReload();

/// Check if packet is WorldReload
bool IsWorldReloadPacket(const uint8_t* data, size_t len);

} // namespace KenshiMP
