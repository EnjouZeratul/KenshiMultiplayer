#include "config.h"
#include <Windows.h>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace fs = std::filesystem;
namespace KenshiMP {

static std::string WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], size, nullptr, nullptr);
    return result;
}

std::string GetLauncherDirectory() {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    fs::path p(path);
    return p.parent_path().string();
}

std::string GetCoreDllPath() {
    fs::path base = GetLauncherDirectory();
    return (base / "KenshiMP_Core.dll").string();
}

LauncherConfig LoadConfig() {
    LauncherConfig cfg;
    cfg.kenshi_path = "";
    cfg.kenshi_exe = "";
    cfg.inject_delay_ms = 3000;
    cfg.launch_without_mp = false;
    cfg.mp_enabled = false;
    cfg.mp_role = "host";
    cfg.mp_host = "127.0.0.1";
    cfg.mp_port = 27960;

    fs::path config_path = fs::path(GetLauncherDirectory()) / "config" / "settings.json";
    if (!fs::exists(config_path))
        return cfg;

    std::ifstream f(config_path);
    if (!f) return cfg;

    std::stringstream buf;
    buf << f.rdbuf();
    std::string content = buf.str();

    // Simple JSON parsing for our needs
    auto extract = [&content](const char* key) -> std::string {
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
    };

    std::string path = extract("kenshi_path");
    if (!path.empty()) {
        cfg.kenshi_path = path;
        cfg.kenshi_exe = (fs::path(path) / "kenshi_x64.exe").string();
    }

    auto extractInt = [&content](const char* key, int def) -> int {
        std::string search = "\"" + std::string(key) + "\"";
        size_t pos = content.find(search);
        if (pos == std::string::npos) return def;
        pos = content.find(':', pos);
        if (pos == std::string::npos) return def;
        while (pos < content.size() && (content[pos] < '0' || content[pos] > '9') && content[pos] != '-') pos++;
        return atoi(content.c_str() + pos);
    };

    cfg.inject_delay_ms = extractInt("inject_delay_ms", 3000);
    cfg.launch_without_mp = extract("launch_without_mp") == "true";
    cfg.mp_enabled = extract("mp_enabled") == "true";
    cfg.mp_role = extract("mp_role");
    if (cfg.mp_role.empty()) cfg.mp_role = "host";
    cfg.mp_host = extract("mp_host");
    if (cfg.mp_host.empty()) cfg.mp_host = "127.0.0.1";
    cfg.mp_port = extractInt("mp_port", 27960);
    cfg.mp_player_name = extract("mp_player_name");
    if (cfg.mp_player_name.size() > 31) cfg.mp_player_name.resize(31);

    return cfg;
}

void SaveConfig(const LauncherConfig& config) {
    fs::path config_dir = fs::path(GetLauncherDirectory()) / "config";
    fs::create_directories(config_dir);

    // 保留 DLL 读取的字段（启动器无 UI），避免覆盖用户手动配置
    std::string client_apply_position = "false";
    auto config_path = config_dir / "settings.json";
    if (fs::exists(config_path)) {
        std::ifstream in(config_path);
        if (in) {
            std::stringstream buf;
            buf << in.rdbuf();
            std::string content = buf.str();
            auto extract = [&content](const char* key) -> std::string {
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
            };
            std::string val = extract("client_apply_position");
            if (!val.empty()) client_apply_position = val;
        }
    }

    std::ofstream f(config_path);
    if (!f) return;

    f << "{\n";
    f << "  \"kenshi_path\": \"" << config.kenshi_path << "\",\n";
    f << "  \"inject_delay_ms\": " << config.inject_delay_ms << ",\n";
    f << "  \"launch_without_mp\": " << (config.launch_without_mp ? "true" : "false") << ",\n";
    f << "  \"mp_enabled\": \"" << (config.mp_enabled ? "true" : "false") << "\",\n";
    f << "  \"mp_role\": \"" << config.mp_role << "\",\n";
    f << "  \"mp_host\": \"" << config.mp_host << "\",\n";
    f << "  \"mp_port\": " << config.mp_port << ",\n";
    f << "  \"mp_player_name\": \"" << config.mp_player_name << "\",\n";
    f << "  \"client_apply_position\": \"" << client_apply_position << "\"\n";
    f << "}\n";
}

} // namespace KenshiMP
