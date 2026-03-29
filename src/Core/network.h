#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace KenshiMP {

// Forward declarations for NAT traversal
class NatTraversal;

struct NetworkConfig {
    uint16_t port = 27960;
    std::string host_address;
    int timeout_ms = 5000;
    bool enable_nat_traversal = true;  // Enable UPnP/STUN for internet connections
    bool enable_upnp = true;           // Enable UPnP port forwarding
    bool enable_stun = true;           // Enable STUN as fallback
};

enum class PeerRole { None, Host, Client };

enum class NatStatus {
    Unknown,
    LocalOnly,           // Only LAN connections possible
    UPnPActive,          // UPnP successful - internet connections possible
    STUNActive,          // STUN successful - internet connections possible
    UnsupportedNAT       // NAT traversal failed - LAN only
};

struct PeerInfo {
    uint64_t peer_id;
    std::string address;
    uint16_t port;
    std::string player_name;  // Client-provided name for slot assignment (IP may change)
};

/// Connection reject reasons
enum class RejectReason : uint32_t { None = 0, ModMismatch = 1 };

/// Connection addresses for sharing with other players
struct ConnectionAddresses {
    std::string public_ip;       // Public IP (for internet players)
    uint16_t public_port = 0;    // Public port
    std::string local_ip;        // Local IP (for LAN players)
    uint16_t local_port = 0;     // Local port
    bool internet_accessible = false;  // Can be reached from internet?

    std::string GetDisplayString() const {
        if (internet_accessible && !public_ip.empty()) {
            return public_ip + ":" + std::to_string(public_port) + " (Internet)";
        }
        return local_ip + ":" + std::to_string(local_port) + " (LAN)";
    }
};

/// Callbacks for network events
struct NetworkCallbacks {
    std::function<void(const PeerInfo&)> on_peer_connected;
    /// peer_id, peer_address, player_name (for reconnect stash key; name preferred over IP)
    std::function<void(uint64_t peer_id, const std::string& address, const std::string& player_name)> on_peer_disconnected;
    std::function<void(uint64_t peer_id, const uint8_t* data, size_t len)> on_data_received;
    std::function<void(RejectReason reason)> on_connection_rejected;
};

/// P2P network layer - UDP based, supports Host and Client roles
/// With NAT traversal support (UPnP -> STUN -> LAN fallback)
class Network {
public:
    Network();
    ~Network();

    /// Initialize NAT traversal (call before StartHost)
    bool InitializeNAT(bool enable_upnp = true, bool enable_stun = true);

    bool StartHost(uint16_t port, uint64_t mod_hash = 0);
    bool StartClient(const std::string& host, uint16_t port, uint64_t mod_hash = 0,
                     const std::string& player_name = "");
    void Stop();

    bool Send(uint64_t peer_id, const uint8_t* data, size_t len);
    void Broadcast(const uint8_t* data, size_t len);

    void SetCallbacks(const NetworkCallbacks& cb) { m_callbacks = cb; }
    void Poll();

    PeerRole GetRole() const { return m_role; }
    bool IsConnected() const { return m_connected; }
    uint64_t GetLocalPeerId() const { return m_local_peer_id; }

    // NAT Traversal information
    NatStatus GetNATStatus() const;
    ConnectionAddresses GetConnectionAddresses() const;
    std::string GetNATStatusMessage() const;
    bool IsInternetAccessible() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    PeerRole m_role = PeerRole::None;
    bool m_connected = false;
    uint64_t m_local_peer_id = 0;
    NetworkCallbacks m_callbacks;
};

} // namespace KenshiMP
