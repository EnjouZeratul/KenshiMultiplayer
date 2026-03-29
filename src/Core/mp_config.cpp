#include "mp_config.h"
#include <Windows.h>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace KenshiMP {

static std::string GetDllDirectory() {
    wchar_t path[MAX_PATH] = {};
    HMODULE hSelf = nullptr;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&LoadMPConfig), &hSelf) && hSelf) {
        GetModuleFileNameW(hSelf, path, MAX_PATH);
    }
    std::filesystem::path p(path);
    return p.parent_path().string();
}

std::string GetMpRootDirectory() {
    return GetDllDirectory();
}

static std::string ExtractJsonString(const std::string& content, const char* key) {
    std::string search = "\"" + std::string(key) + "\"";
    size_t pos = content.find(search);
    if (pos == std::string::npos) return {};
    pos = content.find(':', pos);
    if (pos == std::string::npos) return {};
    pos = content.find('"', pos);
    if (pos == std::string::npos) return {};
    size_t end = content.find('"', pos + 1);
    if (end == std::string::npos) return {};
    return content.substr(pos + 1, end - pos - 1);
}

static int ExtractJsonInt(const std::string& content, const char* key, int default_val) {
    std::string search = "\"" + std::string(key) + "\"";
    size_t pos = content.find(search);
    if (pos == std::string::npos) return default_val;
    pos = content.find(':', pos);
    if (pos == std::string::npos) return default_val;
    while (pos < content.size() && (content[pos] == ' ' || content[pos] == ':')) pos++;
    return atoi(content.c_str() + pos);
}

MPConfig LoadMPConfig() {
    MPConfig cfg;
    cfg.role = "host";
    cfg.host = "127.0.0.1";
    cfg.port = 27960;
    cfg.enabled = false;

    auto config_path = std::filesystem::path(GetDllDirectory()) / "config" / "settings.json";
    if (!std::filesystem::exists(config_path)) return cfg;

    std::ifstream f(config_path);
    if (!f) return cfg;

    std::stringstream buf;
    buf << f.rdbuf();
    std::string content = buf.str();

    std::string role = ExtractJsonString(content, "mp_role");
    if (!role.empty()) cfg.role = role;

    std::string host = ExtractJsonString(content, "mp_host");
    if (!host.empty()) cfg.host = host;

    int port = ExtractJsonInt(content, "mp_port", 27960);
    if (port > 0 && port < 65536) cfg.port = static_cast<uint16_t>(port);

    cfg.enabled = ExtractJsonString(content, "mp_enabled") == "true";
    cfg.player_name = ExtractJsonString(content, "mp_player_name");
    if (cfg.player_name.size() > 31) cfg.player_name.resize(31);
    cfg.client_apply_position = ExtractJsonString(content, "client_apply_position") == "true";

    return cfg;
}

} // namespace KenshiMP
