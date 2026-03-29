#pragma once

#include <string>
#include <vector>
#include <functional>
#include <atomic>

namespace KenshiMP {

/**
 * NAT Traversal Results - priority order from best to worst
 */
enum class NatTraversalResult {
    SuccessUPnP,        // UPnP successful - can accept internet connections
    SuccessSTUN,        // STUN successful with port mapping
    SuccessLocalOnly,   // Local network only (no NAT traversal needed)
    UnsupportedNAT,     // NAT type blocks all traversal attempts
    UpnpFailed,         // UPnP attempted but failed
    StunFailed,         // STUN attempted but failed
    Unknown             // Not yet initialized or error state
};

/**
 * Address information for connection endpoints
 */
struct ConnectionAddress {
    std::string public_ip;      // Public IP (empty if not discovered)
    uint16_t public_port = 0;   // Public port (0 if not mapped)
    std::string local_ip;       // Local IP for LAN connections
    uint16_t local_port = 0;    // Local port for binding
    bool is_public_reachable = false; // Can be reached from internet?

    std::string ToString() const {
        char buf[128];
        if (!public_ip.empty() && is_public_reachable) {
            snprintf(buf, sizeof(buf), "%s:%u (internet)", public_ip.c_str(), public_port);
        } else {
            snprintf(buf, sizeof(buf), "%s:%u (local)", local_ip.c_str(), local_port);
        }
        return std::string(buf);
    }
};

/**
 * Progress callback type for async operations
 */
using NatProgressCallback = std::function<void(const std::string& status_message)>;

/**
 * Main NAT Traversal Manager
 *
 * Provides automatic NAT traversal using the following priority:
 * 1. UPnP - Automatic port forwarding on router
 * 2. STUN - Discover public endpoint through STUN servers
 * 3. LAN mode - Local network only
 *
 * Designed for pure external mounting - no game file modifications.
 */
class NatTraversal {
public:
    NatTraversal();
    ~NatTraversal();

    /**
     * Initialize the NAT traversal system
     * @param enable_upnp Whether to attempt UPnP (default: true)
     * @param enable_stun Whether to attempt STUN fallback (default: true)
     * @param progress_callback Called periodically with status updates
     * @return true if initialization successful
     */
    bool Initialize(bool enable_upnp = true, bool enable_stun = true,
                   NatProgressCallback progress_callback = nullptr);

    /**
     * Shutdown and release resources
     */
    void Shutdown();

    /**
     * Set custom STUN servers (overrides default public servers)
     * @param servers List of STUN server addresses in "ip:port" format
     */
    void SetCustomStunServers(const std::vector<std::string>& servers);

    /**
     * Reset to default public STUN servers
     */
    void ResetToDefaultStunServers();

    /**
     * Get the external (public) address and port for incoming connections
     * @param[out] ip Output: public IP address string
     * @param[out] port Output: public port number
     * @return true if public address discovered
     */
    bool GetExternalAddress(std::string& ip, uint16_t& port);

    /**
     * Get both public and local addresses with reachability info
     * @return ConnectionAddress struct with full address information
     */
    ConnectionAddress GetConnectionAddresses() const;

    /**
     * Get the current NAT traversal result status
     * @return NatTraversalResult enum indicating success/failure mode
     */
    NatTraversalResult GetResult() const;

    /**
     * Check if internet connectivity is available
     * @return true if host can be reached from internet
     */
    bool IsInternetAccessible() const;

    /**
     * Check if local network connectivity is available
     * @return true if LAN connections are possible
     */
    bool IsLANAccessible() const;

    /**
     * Get a human-readable description of current status
     * @return Status message string
     */
    std::string GetStatusMessage() const;

private:
    class Impl;
    Impl* impl_;

    NatTraversal(const NatTraversal&) = delete;
    NatTraversal& operator=(const NatTraversal&) = delete;
};

} // namespace KenshiMP
