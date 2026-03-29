#pragma once

#include <string>
#include <cstdint>

namespace KenshiMP {

struct MPConfig {
    std::string role;       // "host" or "client"
    std::string host;       // For client: server address
    uint16_t port = 27960;
    bool enabled = false;
    /// Client: player name for slot assignment (IP may change, name persists)
    std::string player_name;
    /// Client: apply received host position to local selected char (for sync test)
    bool client_apply_position = false;
};

/// Load from config/settings.json (in DLL's directory)
MPConfig LoadMPConfig();

/// Get KenshiMultiplayer root directory (DLL location)
std::string GetMpRootDirectory();

} // namespace KenshiMP
