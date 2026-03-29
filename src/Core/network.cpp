#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <WinSock2.h>
#include <WS2tcpip.h>
#include "network.h"
#include "logger.h"
#include "mod_check.h"
#include "nat_traversal.h"
#include "local_ip.h"
#include <unordered_map>
#include <random>
#include <chrono>

#pragma comment(lib, "ws2_32.lib")

namespace KenshiMP {

struct Network::Impl {
    SOCKET host_socket = INVALID_SOCKET;
    std::unordered_map<uint64_t, sockaddr_in> peers;
    std::unordered_map<uint64_t, std::chrono::steady_clock::time_point> peer_last_seen;
    std::unordered_map<uint64_t, std::string> peer_player_name;
    uint64_t next_peer_id = 1;
    uint64_t local_peer_id = 0;
    sockaddr_in host_addr{};
    bool is_host = false;
    bool connected = false;
    uint64_t mod_hash = 0;
    static constexpr int PEER_TIMEOUT_SEC = 90;

    // NAT traversal
    std::unique_ptr<NatTraversal> nat_traversal;
    NatStatus nat_status = NatStatus::Unknown;
    ConnectionAddresses connection_addresses;
};

static bool s_wsa_init = false;

static void EnsureWSA() {
    if (!s_wsa_init) {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) == 0)
            s_wsa_init = true;
    }
}

Network::Network() : m_impl(std::make_unique<Impl>()) {
    EnsureWSA();
    std::random_device rd;
    m_impl->local_peer_id = (static_cast<uint64_t>(rd()) << 32) | rd();
    m_local_peer_id = m_impl->local_peer_id;
}

Network::~Network() {
    Stop();
}

bool Network::InitializeNAT(bool enable_upnp, bool enable_stun) {
    m_impl->nat_traversal = std::make_unique<NatTraversal>();

    bool success = m_impl->nat_traversal->Initialize(enable_upnp, enable_stun,
        [](const std::string& msg) {
            Logger::Info("[NAT] " + msg);
        });

    // Update NAT status
    auto result = m_impl->nat_traversal->GetResult();
    switch (result) {
        case NatTraversalResult::SuccessUPnP:
            m_impl->nat_status = NatStatus::UPnPActive;
            break;
        case NatTraversalResult::SuccessSTUN:
            m_impl->nat_status = NatStatus::STUNActive;
            break;
        case NatTraversalResult::SuccessLocalOnly:
        case NatTraversalResult::UpnpFailed:
        case NatTraversalResult::StunFailed:
            m_impl->nat_status = NatStatus::LocalOnly;
            break;
        default:
            m_impl->nat_status = NatStatus::Unknown;
            break;
    }

    // Get connection addresses
    auto addr = m_impl->nat_traversal->GetConnectionAddresses();
    m_impl->connection_addresses.public_ip = addr.public_ip;
    m_impl->connection_addresses.public_port = addr.public_port;
    m_impl->connection_addresses.local_ip = addr.local_ip;
    m_impl->connection_addresses.local_port = addr.local_port;
    m_impl->connection_addresses.internet_accessible = addr.is_public_reachable;

    if (m_impl->nat_status == NatStatus::UPnPActive ||
        m_impl->nat_status == NatStatus::STUNActive) {
        Logger::Info("[NAT] Internet connections enabled - Public IP: " + addr.public_ip);
    } else {
        Logger::Info("[NAT] LAN only mode - Local IP: " + addr.local_ip);
    }

    return success;
}

bool Network::StartHost(uint16_t port, uint64_t mod_hash) {
    Stop();
    m_impl->mod_hash = mod_hash;

    m_impl->host_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_impl->host_socket == INVALID_SOCKET) {
        Logger::Error("socket() failed: " + std::to_string(WSAGetLastError()));
        return false;
    }

    int opt = 1;
    setsockopt(m_impl->host_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    m_impl->host_addr = {};
    m_impl->host_addr.sin_family = AF_INET;
    m_impl->host_addr.sin_addr.s_addr = INADDR_ANY;
    m_impl->host_addr.sin_port = htons(port);

    if (bind(m_impl->host_socket, (sockaddr*)&m_impl->host_addr, sizeof(m_impl->host_addr)) == SOCKET_ERROR) {
        Logger::Error("bind() failed: " + std::to_string(WSAGetLastError()));
        closesocket(m_impl->host_socket);
        m_impl->host_socket = INVALID_SOCKET;
        return false;
    }

    u_long nonblock = 1;
    ioctlsocket(m_impl->host_socket, FIONBIO, &nonblock);

    m_impl->is_host = true;
    m_impl->connected = true;
    m_role = PeerRole::Host;
    m_connected = true;

    // Update connection addresses with actual port
    if (m_impl->nat_traversal) {
        m_impl->connection_addresses.local_port = port;
        m_impl->connection_addresses.public_port = port;

        // Try to add port mapping if UPnP is available
        if (m_impl->nat_status == NatStatus::UPnPActive) {
            Logger::Info("Host started with UPnP - Internet players can connect to: " +
                m_impl->connection_addresses.public_ip + ":" + std::to_string(port));
        } else if (m_impl->nat_status == NatStatus::STUNActive) {
            Logger::Info("Host started with STUN - Public address: " +
                m_impl->connection_addresses.public_ip + ":" + std::to_string(port));
        } else {
            Logger::Info("Host started (LAN only) - Local address: " +
                m_impl->connection_addresses.local_ip + ":" + std::to_string(port));
        }
    } else {
        Logger::Info("Host started on port " + std::to_string(port) +
            " (NAT traversal not initialized)");
    }

    return true;
}

bool Network::StartClient(const std::string& host, uint16_t port, uint64_t mod_hash,
                          const std::string& player_name) {
    Stop();
    m_impl->mod_hash = mod_hash;

    m_impl->host_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_impl->host_socket == INVALID_SOCKET) {
        Logger::Error("socket() failed: " + std::to_string(WSAGetLastError()));
        return false;
    }

    sockaddr_in server = {};
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &server.sin_addr);

    uint64_t host_id = 1;
    m_impl->peers[host_id] = server;
    m_impl->is_host = false;
    m_impl->connected = true;
    m_role = PeerRole::Client;
    m_connected = true;

    // Connect packet: magic(8) + peer_id(8) + mod_hash(8) + name_len(1) + name(0-31) = 25-56 bytes
    size_t name_len = (std::min)(player_name.size(), static_cast<size_t>(31));
    size_t pkt_size = 24 + 1 + name_len;
    std::vector<uint8_t> connect_pkt(pkt_size);
    memcpy(connect_pkt.data(), "KMP_CONN", 8);
    memcpy(connect_pkt.data() + 8, &m_impl->local_peer_id, 8);
    memcpy(connect_pkt.data() + 16, &m_impl->mod_hash, 8);
    connect_pkt[24] = static_cast<uint8_t>(name_len);
    if (name_len > 0) memcpy(connect_pkt.data() + 25, player_name.data(), name_len);
    sendto(m_impl->host_socket, (char*)connect_pkt.data(), (int)pkt_size, 0,
        (sockaddr*)&server, sizeof(server));

    u_long nonblock = 1;
    ioctlsocket(m_impl->host_socket, FIONBIO, &nonblock);

    Logger::Info("Client connecting to " + host + ":" + std::to_string(port) +
        (player_name.empty() ? "" : " as '" + player_name + "'"));
    return true;
}

void Network::Stop() {
    if (m_impl->host_socket != INVALID_SOCKET) {
        closesocket(m_impl->host_socket);
        m_impl->host_socket = INVALID_SOCKET;
    }
    m_impl->peers.clear();
    m_impl->peer_last_seen.clear();
    m_impl->peer_player_name.clear();
    m_impl->connected = false;
    m_role = PeerRole::None;
    m_connected = false;
}

// NAT Traversal information methods
NatStatus Network::GetNATStatus() const {
    return m_impl->nat_status;
}

ConnectionAddresses Network::GetConnectionAddresses() const {
    return m_impl->connection_addresses;
}

std::string Network::GetNATStatusMessage() const {
    if (m_impl->nat_traversal) {
        return m_impl->nat_traversal->GetStatusMessage();
    }
    return "NAT traversal not initialized";
}

bool Network::IsInternetAccessible() const {
    return m_impl->nat_traversal ? m_impl->nat_traversal->IsInternetAccessible() : false;
}

bool Network::Send(uint64_t peer_id, const uint8_t* data, size_t len) {
    auto it = m_impl->peers.find(peer_id);
    if (it == m_impl->peers.end()) return false;

    int sent = sendto(m_impl->host_socket, (char*)data, (int)len, 0,
        (sockaddr*)&it->second, sizeof(it->second));
    return sent == (int)len;
}

void Network::Broadcast(const uint8_t* data, size_t len) {
    for (auto& kv : m_impl->peers) {
        sendto(m_impl->host_socket, (char*)data, (int)len, 0, (sockaddr*)&kv.second, sizeof(kv.second));
    }
}

static std::string GetPeerIP(const sockaddr_in& addr) {
    char buf[64] = {};
    inet_ntop(AF_INET, &addr.sin_addr, buf, sizeof(buf));
    return std::string(buf);
}

void Network::Poll() {
    if (m_impl->host_socket == INVALID_SOCKET) return;

    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

    // Host: check peer timeouts (90s no data = disconnect)
    const int timeout_sec = 90;
    if (m_impl->is_host && m_callbacks.on_peer_disconnected) {
        std::vector<std::pair<uint64_t, std::string>> to_disconnect;
        for (auto& kv : m_impl->peers) {
            uint64_t pid = kv.first;
            auto it = m_impl->peer_last_seen.find(pid);
            if (it != m_impl->peer_last_seen.end()) {
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->second).count();
                if (elapsed >= timeout_sec) {
                    to_disconnect.emplace_back(pid, GetPeerIP(kv.second));
                }
            }
        }
        for (auto& kv2 : to_disconnect) {
            uint64_t pid = kv2.first;
            const std::string& ip = kv2.second;
            std::string pname;
            auto nit = m_impl->peer_player_name.find(pid);
            if (nit != m_impl->peer_player_name.end()) pname = nit->second;
            m_impl->peers.erase(pid);
            m_impl->peer_last_seen.erase(pid);
            m_impl->peer_player_name.erase(pid);
            m_callbacks.on_peer_disconnected(pid, ip, pname);
        }
    }

    // Drain received packets (nonblocking socket)
    char buf[4096];
    sockaddr_in from = {};
    int from_len = sizeof(from);
    while (true) {
        int received = recvfrom(m_impl->host_socket, buf, sizeof(buf), 0, (sockaddr*)&from, &from_len);
        if (received <= 0) break;

        // KMP_CONN
        if (received >= 24 && memcmp(buf, "KMP_CONN", 8) == 0) {
            if (m_impl->is_host) {
                uint64_t client_id, client_mod_hash;
                memcpy(&client_id, buf + 8, 8);
                memcpy(&client_mod_hash, buf + 16, 8);
                bool mod_ok = true;
                if (IsModListCheckEnabled() && m_impl->mod_hash != 0)
                    mod_ok = (client_mod_hash == m_impl->mod_hash);
                if (!mod_ok) {
                    uint8_t reject[12];
                    memcpy(reject, "KMP_REJ", 8);
                    uint32_t reason = static_cast<uint32_t>(RejectReason::ModMismatch);
                    memcpy(reject + 8, &reason, 4);
                    sendto(m_impl->host_socket, (char*)reject, sizeof(reject), 0, (sockaddr*)&from, from_len);
                    Logger::Error("Rejected connection: MOD list mismatch");
                    continue;
                }
                std::string client_name;
                if (received >= 26) {
                    uint8_t name_len = buf[24];
                    if (name_len > 0 && name_len <= 31 && received >= 25 + name_len) {
                        client_name.assign((char*)buf + 25, name_len);
                        for (char& c : client_name) if (c < 32 || c > 126) c = '_';
                    }
                }
                uint64_t peer_id = m_impl->next_peer_id++;
                m_impl->peers[peer_id] = from;
                m_impl->peer_last_seen[peer_id] = std::chrono::steady_clock::now();
                m_impl->peer_player_name[peer_id] = client_name;
                PeerInfo info;
                info.peer_id = peer_id;
                char addr_str[64];
                inet_ntop(AF_INET, &from.sin_addr, addr_str, sizeof(addr_str));
                info.address = addr_str;
                info.port = ntohs(from.sin_port);
                info.player_name = client_name;
                if (m_callbacks.on_peer_connected) m_callbacks.on_peer_connected(info);
                uint8_t ack[32];
                memcpy(ack, "KMP_ACK", 8);
                memcpy(ack + 8, &peer_id, 8);
                memcpy(ack + 16, &m_impl->local_peer_id, 8);
                memcpy(ack + 24, &m_impl->mod_hash, 8);
                sendto(m_impl->host_socket, (char*)ack, sizeof(ack), 0, (sockaddr*)&from, from_len);
            }
            continue;
        }
        // KMP_REJ
        if (received >= 12 && memcmp(buf, "KMP_REJ", 8) == 0 && !m_impl->is_host) {
            uint32_t reason = 0;
            memcpy(&reason, buf + 8, 4);
            if (m_callbacks.on_connection_rejected)
                m_callbacks.on_connection_rejected(static_cast<RejectReason>(reason));
            Logger::Error("Connection rejected by host: reason " + std::to_string(reason));
            continue;
        }
        // KMP_ACK
        if (received >= 32 && memcmp(buf, "KMP_ACK", 8) == 0 && !m_impl->is_host) {
            uint64_t my_id, host_id, host_mod_hash;
            memcpy(&my_id, buf + 8, 8);
            memcpy(&host_id, buf + 16, 8);
            memcpy(&host_mod_hash, buf + 24, 8);
            bool mod_ok = true;
            if (IsModListCheckEnabled() && m_impl->mod_hash != 0)
                mod_ok = (host_mod_hash == m_impl->mod_hash);
            if (!mod_ok) {
                if (m_callbacks.on_connection_rejected)
                    m_callbacks.on_connection_rejected(RejectReason::ModMismatch);
                Logger::Error("Connection rejected: MOD list mismatch with host");
                continue;
            }
            m_impl->peer_last_seen[1] = std::chrono::steady_clock::now();
            if (m_callbacks.on_peer_connected) {
                PeerInfo info;
                info.peer_id = 1;
                info.address = "";
                info.port = 0;
                m_callbacks.on_peer_connected(info);
            }
            continue;
        }
        // Regular data
        uint64_t sender_id = 0;
        for (auto& kv : m_impl->peers) {
            if (kv.second.sin_addr.s_addr == from.sin_addr.s_addr && kv.second.sin_port == from.sin_port) {
                sender_id = kv.first;
                break;
            }
        }
        if (sender_id == 0 && m_impl->is_host) {
            sender_id = m_impl->next_peer_id++;
            m_impl->peers[sender_id] = from;
        }
        if (sender_id != 0) {
            m_impl->peer_last_seen[sender_id] = std::chrono::steady_clock::now();
            if (m_callbacks.on_data_received)
                m_callbacks.on_data_received(sender_id, (uint8_t*)buf, received);
        }
    }
}

} // namespace KenshiMP
