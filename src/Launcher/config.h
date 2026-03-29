#pragma once

#include <string>

namespace KenshiMP {

struct LauncherConfig {
    std::string kenshi_path;      // Path to kenshi_x64.exe directory
    std::string kenshi_exe;       // Full path to kenshi_x64.exe
    int inject_delay_ms;          // Delay before injection (wait for game init)
    bool launch_without_mp;       // Launch game only, no injection
    bool mp_enabled;              // Enable multiplayer in DLL
    std::string mp_role;          // "host" or "client"
    std::string mp_host;          // Host address for client
    int mp_port;                  // Port for host/client
    std::string mp_player_name;   // Client: player name for slot assignment (IP may change)

    // NAT Traversal settings
    bool nat_traversal_enabled = true;   // Enable NAT traversal (UPnP/STUN)
    bool nat_upnp_enabled = true;        // Enable UPnP port forwarding
    bool nat_stun_enabled = true;        // Enable STUN as fallback
    std::string custom_stun_server;      // Optional custom STUN server (ip:port)
};

/// Load config from config/settings.json
LauncherConfig LoadConfig();

/// Save config
void SaveConfig(const LauncherConfig& config);

/// Get path to KenshiMP_Core.dll (next to launcher)
std::string GetCoreDllPath();

/// Get launcher's own directory
std::string GetLauncherDirectory();

} // namespace KenshiMP
