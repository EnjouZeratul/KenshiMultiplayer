#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace KenshiMP {

/**
 * STUN Manager - Lightweight RFC 5389 compliant client
 *
 * Discovers public IP:port by sending STUN binding requests
 * to multiple redundant servers for reliability.
 */
class STUNManager {
public:
    enum class NatType {
        FullCone,         // All external hosts can send to any port
        RestrictedCone,   // Only hosts that peer received from
        PortRestrictedCone, // Further restricted by port
        Symmetric,        // Different mapping for each external host
        Unknown
    };

    STUNManager();
    ~STUNManager();

    /**
     * Default STUN servers (publicly available)
     */
    static const std::vector<std::string>& GetDefaultServers();

    /**
     * Query a STUN server for public address
     * @param stun_server "ip:port" format
     * @param timeout_ms Request timeout in milliseconds
     * @return Public address as "ip:port" string, empty if failed
     */
    std::string QueryServer(const std::string& stun_server, int timeout_ms = 5000);

    /**
     * Query multiple STUN servers and return best result
     * @param servers List of STUN server addresses
     * @param timeout_ms Per-server timeout
     * @return Best discovered public address
     */
    std::string QueryMultipleServers(const std::vector<std::string>& servers,
                                     int timeout_ms = 5000);

    /**
     * Detect NAT type based on STUN response patterns
     * @return NAT type enum or Unknown
     */
    NatType DetectNATType();

    /**
     * Check if STUN manager is initialized
     */
    bool IsInitialized() const;

private:
    class Impl;
    Impl* impl_;

    STUNManager(const STUNManager&) = delete;
    STUNManager& operator=(const STUNManager&) = delete;
};

} // namespace KenshiMP
