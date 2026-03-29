#include "injector.h"
#include "config.h"
#include <Windows.h>
#include <string>
#include <thread>
#include <chrono>
#include <filesystem>

#pragma comment(linker, "/SUBSYSTEM:WINDOWS")

static void ShowError(const std::string& msg) {
    MessageBoxA(nullptr, msg.c_str(), "Kenshi Multiplayer", MB_OK | MB_ICONERROR);
}

static void ShowInfo(const std::string& msg) {
    MessageBoxA(nullptr, msg.c_str(), "Kenshi Multiplayer", MB_OK | MB_ICONINFORMATION);
}

#include "launcher_ui.h"

static int RunLauncherImpl();

static int RunLauncherNoExcept(DWORD* out_exception_code) {
    __try {
        return RunLauncherImpl();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        *out_exception_code = GetExceptionCode();
        return -1;
    }
}

static int RunLauncher() {
    using namespace KenshiMP;
    DWORD exc = 0;
    int r = RunLauncherNoExcept(&exc);
    if (r == -1) {
        ShowError("启动器发生异常，错误代码: " + std::to_string(exc));
        return 1;
    }
    return r;
}

static int RunLauncherImpl() {
    using namespace KenshiMP;

    LauncherConfig config = LoadConfig();

    // Auto-detect Kenshi path if not configured (before dialog)
    if (config.kenshi_path.empty()) {
        wchar_t path[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        std::wstring exe_path(path);
        size_t pos = exe_path.find_last_of(L"\\/");
        if (pos != std::wstring::npos) {
            std::wstring base = exe_path.substr(0, pos);
            // Check same dir (launcher next to game)
            std::wstring kenshi_exe = base + L"\\kenshi_x64.exe";
            if (GetFileAttributesW(kenshi_exe.c_str()) != INVALID_FILE_ATTRIBUTES) {
                char narrow[MAX_PATH] = {};
                WideCharToMultiByte(CP_ACP, 0, base.c_str(), -1, narrow, MAX_PATH, nullptr, nullptr);
                config.kenshi_path = narrow;
                config.kenshi_exe = std::string(narrow) + "\\kenshi_x64.exe";
            } else {
                // Check parent dir (KenshiMultiplayer subfolder inside game dir)
                size_t parent_pos = base.find_last_of(L"\\/");
                if (parent_pos != std::wstring::npos) {
                    std::wstring parent = base.substr(0, parent_pos);
                    kenshi_exe = parent + L"\\kenshi_x64.exe";
                    if (GetFileAttributesW(kenshi_exe.c_str()) != INVALID_FILE_ATTRIBUTES) {
                        char narrow[MAX_PATH] = {};
                        WideCharToMultiByte(CP_ACP, 0, parent.c_str(), -1, narrow, MAX_PATH, nullptr, nullptr);
                        config.kenshi_path = narrow;
                        config.kenshi_exe = std::string(narrow) + "\\kenshi_x64.exe";
                    }
                }
            }
        }
    }

    if (!ShowLauncherDialog(config)) {
        return 0;
    }
    SaveConfig(config);

    if (config.kenshi_exe.empty() || GetFileAttributesA(config.kenshi_exe.c_str()) == INVALID_FILE_ATTRIBUTES) {
        ShowError("未找到 kenshi_x64.exe。\n请将 KenshiMultiplayer 放在 Kenshi 游戏目录下，或在 config/settings.json 中设置 kenshi_path。");
        return 1;
    }

    std::string dll_path = GetCoreDllPath();
    if (GetFileAttributesA(dll_path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        ShowError("未找到 KenshiMP_Core.dll，请确保与启动器在同一目录。");
        return 1;
    }

    // Launch game
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};

    std::string work_dir = std::filesystem::path(config.kenshi_exe).parent_path().string();

    if (!CreateProcessA(config.kenshi_exe.c_str(), nullptr, nullptr, nullptr, FALSE,
            0, nullptr, work_dir.c_str(), &si, &pi)) {
        ShowError("启动游戏失败: " + std::to_string(GetLastError()));
        return 1;
    }

    CloseHandle(pi.hThread);

    if (config.launch_without_mp) {
        CloseHandle(pi.hProcess);
        return 0;
    }

    // Wait for game to initialize
    std::this_thread::sleep_for(std::chrono::milliseconds(config.inject_delay_ms));

    auto result = InjectDLL(pi.dwProcessId, dll_path);
    CloseHandle(pi.hProcess);

    if (!result.success) {
        ShowError("DLL 注入失败: " + result.error_message);
        return 1;
    }

    SaveConfig(config);
    return 0;
}

int RunLauncherEntry() {
    return RunLauncher();
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    return RunLauncher();
}
