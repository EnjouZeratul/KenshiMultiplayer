#pragma once

#include <string>
#include <vector>

namespace KenshiMP {

/**
 * Get all local IPv4 addresses on the machine
 * @return Vector of IP address strings
 */
std::vector<std::string> GetLocalIPv4Addresses();

/**
 * Get a user-friendly display IP address
 * Prioritizes LAN IPs (192.168.x.x, 10.x.x.x) for display
 * Falls back to any available IP or 127.0.0.1
 * @return Display IP string suitable for showing to users
 */
std::string GetDisplayLocalIP();

} // namespace KenshiMP
