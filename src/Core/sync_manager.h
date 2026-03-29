#pragma once

#include "network.h"
#include "game_memory.h"
#include "protocol.h"
#include <atomic>
#include <thread>
#include <mutex>
#include <vector>
#include <deque>
#include <unordered_map>

namespace KenshiMP {

/// Snapshot for interpolation
struct StateSnapshot {
    uint32_t tick;
    float world_time;
    std::vector<CharacterState> chars;
    float timestamp;
};

/// Manages host/client sync: collects state, sends updates, applies received state
class SyncManager {
public:
    SyncManager();
    ~SyncManager();

    void StartHost(uint16_t port, uint64_t mod_hash = 0);
    void StartClient(const std::string& host, uint16_t port, uint64_t mod_hash = 0,
                     const std::string& player_name = "");
    void Stop();

    void Tick();  // Call from game loop / timer

    bool IsHost() const { return m_network.GetRole() == PeerRole::Host; }
    bool IsActive() const { return m_active; }

    /// Client: get interpolated state for rendering; world_time_out receives interpolated game hours
    void GetInterpolatedState(std::vector<CharacterState>& out, float& world_time_out);

    /// Host: 获取客户端上报的金钱（用于校验/扣除判定，不写回客户端）
    int32_t GetClientMoney(uint64_t peer_id) const;

    /// Host: 广播 WorldReload（主机读档时调用，客户端清空快照）
    void BroadcastWorldReload();

private:
    void HostTick();
    void ClientTick();
    void OnPeerConnected(const PeerInfo& info);
    void OnPeerDisconnected(uint64_t peer_id, const std::string& address, const std::string& player_name);
    void OnDataReceived(uint64_t peer_id, const uint8_t* data, size_t len);

    Network m_network;
    std::atomic<bool> m_active{false};
    std::atomic<uint32_t> m_tick{0};
    std::vector<CharacterState> m_local_chars;
    std::deque<StateSnapshot> m_snapshot_buffer;
    std::mutex m_remote_mutex;
    float m_last_send_time = 0.f;
    static constexpr float SEND_INTERVAL = 0.05f;  // 20 Hz
    static constexpr size_t MAX_SNAPSHOTS = 8;
    static constexpr float INTERP_DELAY = 0.1f;  // 100ms interpolation delay

    // Host: peer_id -> 客户端上报的金钱（供校验/扣除判定）
    mutable std::mutex m_client_money_mutex;
    std::unordered_map<uint64_t, int32_t> m_client_money;
    uint32_t m_client_tick = 0;  // Client tick for MoneyReport

    // 各自操控各自角色：Host 聚合所有玩家状态广播
    std::mutex m_client_states_mutex;
    std::unordered_map<uint64_t, std::vector<CharacterState>> m_client_states;  // peer_id -> chars
    std::unordered_map<uint64_t, uint8_t> m_peer_to_slot;  // peer_id -> slot (1,2,3...)
    std::unordered_map<uint64_t, std::string> m_peer_to_address;  // peer_id -> IP (for persist on shutdown)
    std::unordered_map<uint64_t, std::string> m_peer_to_player_name;  // peer_id -> player name
    uint8_t m_next_slot = 1;  // Host 分配 slot 用
    uint8_t m_my_slot = 1;    // Client 自己的 slot（默认 1，收到 SlotAssign 后更新）

    // Client: 上次 InputEvent 位置，用于检测移动并低延迟上报
    float m_last_input_pos[3] = {-1e9f, -1e9f, -1e9f};

    // Host: 上次世界时间，用于检测读档并触发 WorldReload
    float m_last_world_time = -1.f;

    // 断线重连：按 IP 暂存掉线玩家的 slot 与角色状态
    std::unordered_map<std::string, std::pair<uint8_t, std::vector<CharacterState>>> m_disconnected_squads;
};

} // namespace KenshiMP
