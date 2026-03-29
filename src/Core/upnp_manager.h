#pragma once

#include <string>
#include <cstdint>

namespace KenshiMP {

/**
 * UPnP Manager - Universal Plug and Play port mapping
 *
 * Uses miniupnp library to automatically configure port forwarding
 * on compatible routers. Provides clean abstraction layer.
 */
class UPnPManager {
public:
    UPnPManager();
    ~UPnPManager();

    /**
     * Initialize UPnP functionality
     * @return true if UPnP device found and initialized
     */
    bool Initialize();

    /**
     * Shutdown UPnP manager and release resources
     */
    void Shutdown();

    /**
     * Add a port mapping rule
     * @param external_port Port to map externally
     * @param internal_port Port to forward internally
     * @param protocol "TCP" or "UDP" (default: UDP)
     * @param lease_duration Mapping lease in seconds (0 = permanent)
     * @param description Optional description for the mapping
     * @return true if port mapping successful
     */
    bool AddPortMapping(uint16_t external_port, uint16_t internal_port,
                       const std::string& protocol = "UDP",
                       unsigned int lease_duration = 3600,
                       const std::string& description = "KenshiMP");

    /**
     * Remove a port mapping rule
     * @param external_port External port to unmap
     * @param protocol Protocol to remove ("TCP" or "UDP")
     * @return true if removal successful
     */
    bool DeletePortMapping(uint16_t external_port,
                          const std::string& protocol = "UDP");

    /**
     * Get the external IP address discovered by UPnP device
     * @return External IP string, empty if not available
     */
    std::string GetExternalIPAddress() const;

    /**
     * Check if UPnP is currently active and available
     * @return true if UPnP device is online
     */
    bool IsAvailable() const;

    /**
     * Get human-readable status message
     * @return Status description
     */
    std::string GetStatusMessage() const;

private:
    class Impl;
    Impl* impl_;

    UPnPManager(const UPnPManager&) = delete;
    UPnPManager& operator=(const UPnPManager&) = delete;
};

} // namespace KenshiMP
