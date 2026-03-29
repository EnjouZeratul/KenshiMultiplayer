#include "logger.h"
#include <Windows.h>
#include <fstream>
#include <mutex>
#include <ctime>
#include <filesystem>

namespace KenshiMP {

static std::mutex s_log_mutex;
static std::ofstream s_log_file;
static std::string s_log_path;

static std::string GetTimestamp() {
    time_t now = time(nullptr);
    struct tm t;
    localtime_s(&t, &now);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &t);
    return buf;
}

void Logger::Init() {
    wchar_t path[MAX_PATH];
    HMODULE hSelf = nullptr;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&Logger::Init), &hSelf) && hSelf) {
        GetModuleFileNameW(hSelf, path, MAX_PATH);
    } else {
        GetModuleFileNameW(nullptr, path, MAX_PATH);
    }
    std::filesystem::path dll_path(path);
    std::filesystem::path log_dir = dll_path.parent_path() / "logs";
    std::filesystem::create_directories(log_dir);
    s_log_path = (log_dir / "kenshi_mp.log").string();
    s_log_file.open(s_log_path, std::ios::app);
}

void Logger::Shutdown() {
    if (s_log_file.is_open())
        s_log_file.close();
}

void Logger::Info(const std::string& msg) {
    std::lock_guard<std::mutex> lock(s_log_mutex);
    if (s_log_file.is_open()) {
        s_log_file << "[" << GetTimestamp() << "] [INFO] " << msg << "\n";
        s_log_file.flush();
    }
    OutputDebugStringA(("[KenshiMP] " + msg + "\n").c_str());
}

void Logger::Error(const std::string& msg) {
    std::lock_guard<std::mutex> lock(s_log_mutex);
    if (s_log_file.is_open()) {
        s_log_file << "[" << GetTimestamp() << "] [ERROR] " << msg << "\n";
        s_log_file.flush();
    }
    OutputDebugStringA(("[KenshiMP ERROR] " + msg + "\n").c_str());
}

void Logger::Debug(const std::string& msg) {
    std::lock_guard<std::mutex> lock(s_log_mutex);
    if (s_log_file.is_open()) {
        s_log_file << "[" << GetTimestamp() << "] [DEBUG] " << msg << "\n";
        s_log_file.flush();
    }
}

} // namespace KenshiMP
