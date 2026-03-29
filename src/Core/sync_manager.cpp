#define NOMINMAX
#include "sync_manager.h"
#include "logger.h"
#include "mod_check.h"
#include "mp_config.h"
#include "game_memory.h"
#include <chrono>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <filesystem>

namespace KenshiMP {

static std::string MakeStashKey(const std::string& player_name, const std::string& address);

static float GetTime() {
    return static_cast<float>(std::chrono::steady_clock::now().time_since_epoch().count()) / 1e9f;
}

static constexpr uint8_t PERSIST_VERSION = 1;
static const char* PERSIST_FILENAME = "saves/mp_reconnect.dat";

using PersistedMap = std::unordered_map<std::string, std::pair<uint8_t, std::vector<CharacterState>>>;

static std::string GetPersistPath() {
    return (std::filesystem::path(GetMpRootDirectory()) / PERSIST_FILENAME).string();
}

static PersistedMap LoadPersistedSquads() {
    PersistedMap out;
    std::string path = GetPersistPath();
    std::ifstream f(path, std::ios::binary);
    if (!f) return out;
    uint8_t ver = 0;
    if (!f.read(reinterpret_cast<char*>(&ver), 1) || ver != PERSIST_VERSION) return out;
    while (f) {
        uint8_t ipLen = 0;
        if (!f.read(reinterpret_cast<char*>(&ipLen), 1) || ipLen == 0 || ipLen > 64) break;
        std::string ip(ipLen, '\0');
        if (!f.read(&ip[0], ipLen)) break;
        uint8_t slot = 0;
        if (!f.read(reinterpret_cast<char*>(&slot), 1)) break;
        uint32_t count = 0;
        if (!f.read(reinterpret_cast<char*>(&count), 4) || count > 64) break;
        std::vector<CharacterState> chars;
        chars.resize(count);
        for (uint32_t i = 0; i < count; ++i) {
            if (!f.read(reinterpret_cast<char*>(&chars[i]), sizeof(CharacterState))) break;
        }
        out[ip] = {slot, std::move(chars)};
    }
    return out;
}

static void SavePersistedSquads(const PersistedMap& map) {
    std::string path = GetPersistPath();
    std::filesystem::path p(path);
    std::filesystem::create_directories(p.parent_path());
    std::ofstream f(path, std::ios::binary);
    if (!f) return;
    f.write(reinterpret_cast<const char*>(&PERSIST_VERSION), 1);
    for (const auto& kv : map) {
        const std::string& ip = kv.first;
        uint8_t ipLen = static_cast<uint8_t>(std::min(ip.size(), size_t(64)));
        if (ipLen == 0) continue;
        f.write(reinterpret_cast<const char*>(&ipLen), 1);
        f.write(ip.data(), ipLen);
        f.write(reinterpret_cast<const char*>(&kv.second.first), 1);
        uint32_t count = static_cast<uint32_t>(kv.second.second.size());
        f.write(reinterpret_cast<const char*>(&count), 4);
        for (const auto& c : kv.second.second)
            f.write(reinterpret_cast<const char*>(&c), sizeof(CharacterState));
    }
}

SyncManager::SyncManager() {
    NetworkCallbacks cb;
    cb.on_peer_connected = [this](const PeerInfo& info) { OnPeerConnected(info); };
    cb.on_peer_disconnected = [this](uint64_t id, const std::string& addr, const std::string& name) {
        OnPeerDisconnected(id, addr, name);
    };
    cb.on_data_received = [this](uint64_t id, const uint8_t* d, size_t len) { OnDataReceived(id, d, len); };
    cb.on_connection_rejected = [this](RejectReason reason) {
        if (reason == RejectReason::ModMismatch)
            Logger::Error("Connection rejected: MOD list mismatch. Ensure host and client have identical mods and load order.");
    };
    m_network.SetCallbacks(cb);
}

SyncManager::~SyncManager() {
    Stop();
}

void SyncManager::StartHost(uint16_t port, uint64_t mod_hash) {
    if (m_network.StartHost(port, mod_hash)) {
        m_active = true;
        Logger::Info("SyncManager: Host started (mod_hash=" + std::to_string(mod_hash) + ")");
    }
}

void SyncManager::StartClient(const std::string& host, uint16_t port, uint64_t mod_hash,
                              const std::string& player_name) {
    if (m_network.StartClient(host, port, mod_hash, player_name)) {
        m_active = true;
        Logger::Info("SyncManager: Client started (mod_hash=" + std::to_string(mod_hash) + ")");
    }
}

void SyncManager::Stop() {
    if (IsHost() && m_active) {
        std::lock_guard<std::mutex> lock(m_client_states_mutex);
        PersistedMap merged = LoadPersistedSquads();
        for (const auto& kv : m_client_states) {
            auto addrIt = m_peer_to_address.find(kv.first);
            auto slotIt = m_peer_to_slot.find(kv.first);
            auto nameIt = m_peer_to_player_name.find(kv.first);
            if (addrIt != m_peer_to_address.end() && slotIt != m_peer_to_slot.end() && slotIt->second > 0) {
                std::string key = MakeStashKey(nameIt != m_peer_to_player_name.end() ? nameIt->second : "", addrIt->second);
                merged[key] = {slotIt->second, kv.second};
            }
        }
        if (!merged.empty()) SavePersistedSquads(merged);
    }
    m_active = false;
    m_network.Stop();
}

void SyncManager::Tick() {
    if (!m_active) return;

    m_network.Poll();

    if (IsHost()) {
        HostTick();
    } else {
        ClientTick();
    }
}

void SyncManager::HostTick() {
    m_tick++;

    // 聚合：主机角色 (slot 0) + 各客户端角色 (slot 1, 2, ...)
    std::vector<CharacterState> combined;
    uintptr_t selected = GameMemory::GetSelectedCharacterBase();
    if (selected != 0) {
        auto members = GameMemory::EnumerateSquadMembers(selected);
        for (uintptr_t ptr : members) {
            auto state = GameMemory::ReadCharacter(ptr);
            if (state) {
                state->player_slot = 0;
                combined.push_back(*state);
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_client_states_mutex);
        for (const auto& kv : m_client_states) {
            uint64_t peer_id = kv.first;
            uint8_t slot = 0;
            auto it = m_peer_to_slot.find(peer_id);
            if (it != m_peer_to_slot.end()) slot = it->second;
            for (const auto& c : kv.second) {
                CharacterState cs = c;
                cs.player_slot = slot;
                combined.push_back(cs);
            }
        }
    }

    auto world_time = GameMemory::ReadWorldTime().value_or(0.f);

    // 检测读档：世界时间大幅回退（排除 24h 日循环）
    if (m_last_world_time > 0.f && world_time < m_last_world_time - 2.f
        && !(m_last_world_time > 22.f && world_time < 2.f)) {
        BroadcastWorldReload();
    }
    m_last_world_time = world_time;

    auto packet = SerializeStateUpdate(m_tick.load(), world_time,
        combined.empty() ? nullptr : combined.data(),
        static_cast<uint32_t>(combined.size()));

    if (!packet.empty()) {
        m_network.Broadcast(packet.data(), packet.size());
    }

    // 主机本地应用客户端角色到小队成员 1, 2... 以便看到对方（按 slot 顺序）
    if (selected != 0) {
        auto members = GameMemory::EnumerateSquadMembers(selected);
        std::lock_guard<std::mutex> lock(m_client_states_mutex);
        std::vector<std::pair<uint8_t, const std::vector<CharacterState>*>> by_slot;
        for (const auto& kv : m_client_states) {
            auto it = m_peer_to_slot.find(kv.first);
            uint8_t slot = (it != m_peer_to_slot.end()) ? it->second : 0;
            by_slot.push_back({slot, &kv.second});
        }
        std::sort(by_slot.begin(), by_slot.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });
        size_t member_idx = 1;
        for (const auto& p : by_slot) {
            if (member_idx >= members.size()) break;
            if (!p.second->empty()) {
                const auto& c = (*p.second)[0];
                GameMemory::WriteCharacterPosition(members[member_idx], c.pos_x, c.pos_y, c.pos_z);
                GameMemory::WriteCharacterHP(members[member_idx], c.hp_current);
                member_idx++;
            }
        }
    }
}

void SyncManager::ClientTick() {
    m_client_tick++;

    uintptr_t selected = GameMemory::GetSelectedCharacterBase();
    if (selected != 0) {
        std::vector<CharacterState> my_chars;
        auto members = GameMemory::EnumerateSquadMembers(selected);
        for (uintptr_t ptr : members) {
            auto state = GameMemory::ReadCharacter(ptr);
            if (state) {
                state->player_slot = m_my_slot;
                my_chars.push_back(*state);
            }
        }
        if (!my_chars.empty()) {
            const auto& c = my_chars[0];
            float dx = c.pos_x - m_last_input_pos[0], dy = c.pos_y - m_last_input_pos[1], dz = c.pos_z - m_last_input_pos[2];
            float dist_sq = dx * dx + dy * dy + dz * dz;
            if (dist_sq > 0.25f) {  // 移动超过 0.5 单位即发送，降低延迟
                InputEventPayload ev = {};
                ev.tick = m_client_tick;
                ev.event_type = 0;
                ev.pos_x = c.pos_x;
                ev.pos_y = c.pos_y;
                ev.pos_z = c.pos_z;
                ev.target_id = 0;
                auto pkt = SerializeInputEvent(ev);
                if (!pkt.empty()) m_network.Send(1, pkt.data(), pkt.size());
                m_last_input_pos[0] = c.pos_x;
                m_last_input_pos[1] = c.pos_y;
                m_last_input_pos[2] = c.pos_z;
            }
            if (m_client_tick % 2 == 0) {  // 约 10 Hz 完整状态
                auto pkt = SerializeClientStateReport(m_client_tick, my_chars.data(),
                    static_cast<uint32_t>(my_chars.size()));
                if (!pkt.empty()) m_network.Send(1, pkt.data(), pkt.size());
            }
        }
    }

    // 金钱上报
    if (m_client_tick % 20 == 0) {
        auto money = GameMemory::ReadMoney();
        if (money) {
            MoneyReportPayload rep;
            rep.money = *money;
            rep.tick = m_client_tick;
            auto pkt = SerializeMoneyReport(rep);
            if (!pkt.empty()) m_network.Send(1, pkt.data(), pkt.size());
        }
    }

    // 应用远程角色到小队成员 1, 2...（不覆盖自己的角色）
    auto cfg = LoadMPConfig();
    std::vector<CharacterState> interp;
    float world_time = 0.f;
    GetInterpolatedState(interp, world_time);
    if (world_time > 0.f) GameMemory::WriteWorldTime(world_time);

    if (cfg.client_apply_position && !interp.empty()) {
        uintptr_t selected = GameMemory::GetSelectedCharacterBase();
        if (selected != 0) {
            auto members = GameMemory::EnumerateSquadMembers(selected);
            // 收集远程角色（slot != m_my_slot），按 slot 排序后应用到 members[1], [2]...
            std::vector<CharacterState> remotes;
            for (const auto& c : interp) {
                if (c.player_slot != m_my_slot) remotes.push_back(c);
            }
            std::sort(remotes.begin(), remotes.end(),
                [](const auto& a, const auto& b) { return a.player_slot < b.player_slot; });
            for (size_t i = 0; i < remotes.size() && i + 1 < members.size(); ++i) {
                const auto& c = remotes[i];
                GameMemory::WriteCharacterPosition(members[i + 1], c.pos_x, c.pos_y, c.pos_z);
                GameMemory::WriteCharacterHP(members[i + 1], c.hp_current);
            }
        }
    }
}

static std::string MakeStashKey(const std::string& player_name, const std::string& address) {
    if (!player_name.empty()) return player_name;
    return "IP:" + address;
}

void SyncManager::OnPeerConnected(const PeerInfo& info) {
    std::string log_name = info.player_name.empty() ? info.address : ("'" + info.player_name + "' from " + info.address);
    Logger::Info("Peer connected: " + std::to_string(info.peer_id) + " " + log_name);
    if (IsHost()) {
        std::lock_guard<std::mutex> lock(m_client_states_mutex);
        std::string key = MakeStashKey(info.player_name, info.address);
        uint8_t slot;
        auto it = m_disconnected_squads.find(key);
        if (it != m_disconnected_squads.end()) {
            slot = it->second.first;
            m_client_states[info.peer_id] = it->second.second;
            m_disconnected_squads.erase(it);
            Logger::Info("Reconnect: restored slot " + std::to_string(slot) + " for " + key);
        } else {
            it = m_disconnected_squads.find(info.address);
            if (it != m_disconnected_squads.end()) {
                slot = it->second.first;
                m_client_states[info.peer_id] = it->second.second;
                m_disconnected_squads.erase(it);
                Logger::Info("Reconnect (IP fallback): restored slot " + std::to_string(slot) + " for " + info.address);
            } else {
                auto persisted = LoadPersistedSquads();
                auto pit = persisted.find(key);
                if (pit != persisted.end()) {
                    slot = pit->second.first;
                    m_client_states[info.peer_id] = pit->second.second;
                    Logger::Info("Reconnect (persisted): restored slot " + std::to_string(slot) + " for " + key);
                } else {
                    pit = persisted.find("IP:" + info.address);
                    if (pit != persisted.end()) {
                        slot = pit->second.first;
                        m_client_states[info.peer_id] = pit->second.second;
                        Logger::Info("Reconnect (persisted IP): restored slot " + std::to_string(slot) + " for " + info.address);
                    } else {
                        slot = m_next_slot++;
                    }
                }
            }
        }
        m_peer_to_slot[info.peer_id] = slot;
        m_peer_to_address[info.peer_id] = info.address;
        m_peer_to_player_name[info.peer_id] = info.player_name;
        auto pkt = SerializeSlotAssign(slot);
        if (!pkt.empty()) m_network.Send(info.peer_id, pkt.data(), pkt.size());
    }
}

void SyncManager::OnPeerDisconnected(uint64_t peer_id, const std::string& address, const std::string& player_name) {
    Logger::Info("Peer disconnected: " + std::to_string(peer_id) + " (" + address + ")");
    if (IsHost()) {
        std::lock_guard<std::mutex> lock(m_client_states_mutex);
        auto slot_it = m_peer_to_slot.find(peer_id);
        uint8_t slot = (slot_it != m_peer_to_slot.end()) ? slot_it->second : 0;
        auto states_it = m_client_states.find(peer_id);
        std::string stash_name = player_name;
        if (stash_name.empty()) {
            auto nit = m_peer_to_player_name.find(peer_id);
            if (nit != m_peer_to_player_name.end()) stash_name = nit->second;
        }
        std::string key = MakeStashKey(stash_name, address);
        if (slot > 0 && states_it != m_client_states.end()) {
            m_disconnected_squads[key] = {slot, states_it->second};
            Logger::Info("Stashed " + std::to_string(states_it->second.size()) + " chars for reconnect (" + key + ")");
            auto merged = LoadPersistedSquads();
            for (const auto& kv : m_disconnected_squads) merged[kv.first] = kv.second;
            SavePersistedSquads(merged);
        }
        m_client_states.erase(peer_id);
        m_peer_to_slot.erase(peer_id);
        m_peer_to_address.erase(peer_id);
        m_peer_to_player_name.erase(peer_id);
    }
}

void SyncManager::GetInterpolatedState(std::vector<CharacterState>& out, float& world_time_out) {
    out.clear();
    world_time_out = 0.f;
    std::lock_guard<std::mutex> lock(m_remote_mutex);
    if (m_snapshot_buffer.size() < 2) {
        if (!m_snapshot_buffer.empty()) {
            out = m_snapshot_buffer.back().chars;
            world_time_out = m_snapshot_buffer.back().world_time;
        }
        return;
    }

    float t = GetTime() - INTERP_DELAY;
    const StateSnapshot* prev = nullptr;
    const StateSnapshot* next = nullptr;

    for (size_t i = 0; i + 1 < m_snapshot_buffer.size(); ++i) {
        if (m_snapshot_buffer[i].timestamp <= t && t <= m_snapshot_buffer[i + 1].timestamp) {
            prev = &m_snapshot_buffer[i];
            next = &m_snapshot_buffer[i + 1];
            break;
        }
    }
    if (!prev) prev = &m_snapshot_buffer.front();
    if (!next) next = &m_snapshot_buffer.back();

    float denom = next->timestamp - prev->timestamp;
    float alpha = (denom > 1e-6f) ? (t - prev->timestamp) / denom : 1.f;
    alpha = std::max(0.f, std::min(1.f, alpha));
    world_time_out = prev->world_time + (next->world_time - prev->world_time) * alpha;

    size_t n = std::min(prev->chars.size(), next->chars.size());
    out.resize(n);
    for (size_t i = 0; i < n; ++i) {
        auto& p = prev->chars[i];
        auto& q = next->chars[i];
        out[i].character_id = p.character_id;
        out[i].squad_id = p.squad_id;
        out[i].pos_x = p.pos_x + (q.pos_x - p.pos_x) * alpha;
        out[i].pos_y = p.pos_y + (q.pos_y - p.pos_y) * alpha;
        out[i].pos_z = p.pos_z + (q.pos_z - p.pos_z) * alpha;
        out[i].rot_y = p.rot_y + (q.rot_y - p.rot_y) * alpha;
        out[i].hp_current = p.hp_current + (q.hp_current - p.hp_current) * alpha;
        out[i].hp_max = p.hp_max;
        out[i].player_slot = p.player_slot;
        memcpy(out[i].name, p.name, sizeof(p.name));
    }
}

void SyncManager::OnDataReceived(uint64_t peer_id, const uint8_t* data, size_t len) {
    if (len < sizeof(PacketHeader)) return;

    auto* hdr = reinterpret_cast<const PacketHeader*>(data);
    if (hdr->magic[0] != 'K' || hdr->magic[1] != 'M' || hdr->magic[2] != 'P') return;

    if (hdr->type == PacketType::StateUpdate) {
        uint32_t tick;
        float world_time;
        CharacterState chars[64];
        uint32_t count = 0;

        if (DeserializeStateUpdate(data, len, tick, world_time, chars, count, 64)) {
            StateSnapshot snap;
            snap.tick = tick;
            snap.world_time = world_time;
            snap.chars.assign(chars, chars + count);
            snap.timestamp = GetTime();

            std::lock_guard<std::mutex> lock(m_remote_mutex);
            m_snapshot_buffer.push_back(snap);
            while (m_snapshot_buffer.size() > MAX_SNAPSHOTS)
                m_snapshot_buffer.pop_front();
        }
    } else if (hdr->type == PacketType::InputEvent) {
        auto ev = DeserializeInputEvent(data, len);
        if (ev && IsHost()) {
            std::lock_guard<std::mutex> lock(m_client_states_mutex);
            auto it = m_client_states.find(peer_id);
            if (it != m_client_states.end() && !it->second.empty()) {
                it->second[0].pos_x = ev->pos_x;
                it->second[0].pos_y = ev->pos_y;
                it->second[0].pos_z = ev->pos_z;
            }
        }
    } else if (hdr->type == PacketType::MoneyReport) {
        auto ev = DeserializeMoneyReport(data, len);
        if (ev && IsHost()) {
            std::lock_guard<std::mutex> lock(m_client_money_mutex);
            m_client_money[peer_id] = ev->money;
        }
    } else if (hdr->type == PacketType::ClientStateReport) {
        uint32_t tick;
        CharacterState chars[64];
        uint32_t count = 0;
        if (DeserializeClientStateReport(data, len, tick, chars, count, 64) && IsHost()) {
            std::lock_guard<std::mutex> lock(m_client_states_mutex);
            m_client_states[peer_id].assign(chars, chars + count);
        }
    } else if (hdr->type == PacketType::SlotAssign) {
        auto slot = DeserializeSlotAssign(data, len);
        if (slot && !IsHost()) m_my_slot = *slot;
    } else if (IsWorldReloadPacket(data, len) && !IsHost()) {
        std::lock_guard<std::mutex> lock(m_remote_mutex);
        m_snapshot_buffer.clear();
        Logger::Info("WorldReload: cleared snapshot buffer");
    }
}

void SyncManager::BroadcastWorldReload() {
    if (!IsHost()) return;
    auto pkt = SerializeWorldReload();
    if (!pkt.empty()) m_network.Broadcast(pkt.data(), pkt.size());
    Logger::Info("Broadcast WorldReload");
}

int32_t SyncManager::GetClientMoney(uint64_t peer_id) const {
    std::lock_guard<std::mutex> lock(m_client_money_mutex);
    auto it = m_client_money.find(peer_id);
    return (it != m_client_money.end()) ? it->second : -1;
}

} // namespace KenshiMP
