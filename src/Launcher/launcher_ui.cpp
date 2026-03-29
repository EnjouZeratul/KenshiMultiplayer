#define NOMINMAX
#include "launcher_ui.h"
#include "local_ip.h"
#include <Windows.h>
#include <CommCtrl.h>
#include <string>
#include <Shellapi.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

namespace KenshiMP {

static LauncherConfig* s_config = nullptr;
static HWND s_hMain = nullptr;
static HWND s_hCheckMP = nullptr;
static HWND s_hRadioHost = nullptr;
static HWND s_hRadioClient = nullptr;
static HWND s_hEditHost = nullptr;
static HWND s_hEditPort = nullptr;
static HWND s_hStaticHostIP = nullptr;
static HWND s_hStaticHostLabel = nullptr;
static HWND s_hEditPlayerName = nullptr;
static HWND s_hStaticPlayerName = nullptr;
static HWND s_hCheckNoMP = nullptr;

// NAT Traversal UI controls
static HWND s_hCheckNatEnabled = nullptr;
static HWND s_hCheckNatUpnp = nullptr;
static HWND s_hCheckNatStun = nullptr;
static HWND s_hEditStunServer = nullptr;
static HWND s_hStaticStunServer = nullptr;

static bool s_launch = false;

static void ToWide(const std::string& s, std::wstring& out) {
    if (s.empty()) { out.clear(); return; }
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    out.resize(n);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &out[0], n);
}

static std::string FromWide(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string s(n - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], n, nullptr, nullptr);
    return s;
}

static void SaveFromControls() {
    if (!s_config || !s_hMain) return;

    s_config->launch_without_mp = (SendMessage(s_hCheckNoMP, BM_GETCHECK, 0, 0) == BST_CHECKED);
    s_config->mp_enabled = !s_config->launch_without_mp && (SendMessage(s_hCheckMP, BM_GETCHECK, 0, 0) == BST_CHECKED);

    if (SendMessage(s_hRadioHost, BM_GETCHECK, 0, 0) == BST_CHECKED)
        s_config->mp_role = "host";
    else
        s_config->mp_role = "client";

    wchar_t buf[256] = {};
    GetWindowTextW(s_hEditHost, buf, 256);
    s_config->mp_host = FromWide(buf);
    if (s_config->mp_host.empty()) s_config->mp_host = "127.0.0.1";

    GetWindowTextW(s_hEditPort, buf, 256);
    s_config->mp_port = _wtoi(buf);
    if (s_config->mp_port <= 0) s_config->mp_port = 27960;

    if (s_hEditPlayerName) {
        GetWindowTextW(s_hEditPlayerName, buf, 256);
        s_config->mp_player_name = FromWide(buf);
        if (s_config->mp_player_name.size() > 31) s_config->mp_player_name.resize(31);
    }

    // NAT Traversal settings
    if (s_hCheckNatEnabled) {
        s_config->nat_traversal_enabled = (SendMessage(s_hCheckNatEnabled, BM_GETCHECK, 0, 0) == BST_CHECKED);
    }
    if (s_hCheckNatUpnp) {
        s_config->nat_upnp_enabled = (SendMessage(s_hCheckNatUpnp, BM_GETCHECK, 0, 0) == BST_CHECKED);
    }
    if (s_hCheckNatStun) {
        s_config->nat_stun_enabled = (SendMessage(s_hCheckNatStun, BM_GETCHECK, 0, 0) == BST_CHECKED);
    }
    if (s_hEditStunServer) {
        GetWindowTextW(s_hEditStunServer, buf, 256);
        s_config->custom_stun_server = FromWide(buf);
    }
}

static void UpdateHostIPDisplay() {
    if (!s_hStaticHostIP) return;
    bool isHost = (SendMessage(s_hRadioHost, BM_GETCHECK, 0, 0) == BST_CHECKED);
    if (isHost) {
        std::string ip = GetDisplayLocalIP();
        wchar_t buf[256] = {};
        int port = 27960;
        if (s_hEditPort) {
            wchar_t pw[32] = {};
            GetWindowTextW(s_hEditPort, pw, 32);
            port = _wtoi(pw);
            if (port <= 0) port = 27960;
        }
        swprintf_s(buf, L"您的 IP: %S  端口: %d\n请将上述信息告知加入者", ip.c_str(), port);
        SetWindowTextW(s_hStaticHostIP, buf);
        ShowWindow(s_hStaticHostIP, SW_SHOW);
        if (s_hEditHost) ShowWindow(s_hEditHost, SW_HIDE);
        if (s_hStaticHostLabel) ShowWindow(s_hStaticHostLabel, SW_HIDE);
    } else {
        ShowWindow(s_hStaticHostIP, SW_HIDE);
        if (s_hEditHost) ShowWindow(s_hEditHost, SW_SHOW);
        if (s_hStaticHostLabel) ShowWindow(s_hStaticHostLabel, SW_SHOW);
    }
    if (s_hEditPlayerName && s_hStaticPlayerName) {
        bool show = (SendMessage(s_hRadioClient, BM_GETCHECK, 0, 0) == BST_CHECKED);
        ShowWindow(s_hStaticPlayerName, show ? SW_SHOW : SW_HIDE);
        ShowWindow(s_hEditPlayerName, show ? SW_SHOW : SW_HIDE);
    }
}

static void LoadToControls() {
    if (!s_config || !s_hMain) return;

    SendMessage(s_hCheckNoMP, BM_SETCHECK, s_config->launch_without_mp ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(s_hCheckMP, BM_SETCHECK, s_config->mp_enabled ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(s_hRadioHost, BM_SETCHECK, (s_config->mp_role == "host") ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(s_hRadioClient, BM_SETCHECK, (s_config->mp_role == "client") ? BST_CHECKED : BST_UNCHECKED, 0);

    std::wstring whost, wport, wname;
    ToWide(s_config->mp_host, whost);
    ToWide(std::to_string(s_config->mp_port), wport);
    ToWide(s_config->mp_player_name.empty() ? "Player1" : s_config->mp_player_name, wname);
    SetWindowTextW(s_hEditHost, whost.c_str());
    SetWindowTextW(s_hEditPort, wport.c_str());
    if (s_hEditPlayerName) SetWindowTextW(s_hEditPlayerName, wname.c_str());
    UpdateHostIPDisplay();

    // NAT Traversal settings
    if (s_hCheckNatEnabled) {
        SendMessage(s_hCheckNatEnabled, BM_SETCHECK, s_config->nat_traversal_enabled ? BST_CHECKED : BST_UNCHECKED, 0);
    }
    if (s_hCheckNatUpnp) {
        SendMessage(s_hCheckNatUpnp, BM_SETCHECK, s_config->nat_upnp_enabled ? BST_CHECKED : BST_UNCHECKED, 0);
    }
    if (s_hCheckNatStun) {
        SendMessage(s_hCheckNatStun, BM_SETCHECK, s_config->nat_stun_enabled ? BST_CHECKED : BST_UNCHECKED, 0);
    }
    if (s_hEditStunServer) {
        std::wstring wstun;
        ToWide(s_config->custom_stun_server.empty() ? "stun.l.google.com:19302" : s_config->custom_stun_server, wstun);
        SetWindowTextW(s_hEditStunServer, wstun.c_str());
    }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        s_hCheckNoMP = CreateWindowW(L"BUTTON", L"仅启动游戏（不注入联机）",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 20, 20, 280, 24, hwnd, nullptr, nullptr, nullptr);
        s_hCheckMP = CreateWindowW(L"BUTTON", L"启用联机",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 20, 48, 120, 24, hwnd, nullptr, nullptr, nullptr);
        s_hRadioHost = CreateWindowW(L"BUTTON", L"主机",
            WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 40, 76, 80, 24, hwnd, (HMENU)10, nullptr, nullptr);
        s_hRadioClient = CreateWindowW(L"BUTTON", L"客户端",
            WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 130, 76, 80, 24, hwnd, (HMENU)11, nullptr, nullptr);
        s_hStaticHostLabel = CreateWindowW(L"STATIC", L"主机地址:", WS_CHILD | WS_VISIBLE, 40, 104, 70, 24, hwnd, nullptr, nullptr, nullptr);
        s_hEditHost = CreateWindowW(L"EDIT", L"127.0.0.1",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 120, 102, 180, 24, hwnd, nullptr, nullptr, nullptr);
        s_hStaticHostIP = CreateWindowW(L"STATIC", L"",
            WS_CHILD | WS_VISIBLE, 40, 104, 280, 48, hwnd, nullptr, nullptr, nullptr);
        CreateWindowW(L"STATIC", L"端口:", WS_CHILD | WS_VISIBLE, 40, 132, 40, 24, hwnd, nullptr, nullptr, nullptr);
        s_hEditPort = CreateWindowW(L"EDIT", L"27960",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 120, 130, 80, 24, hwnd, nullptr, nullptr, nullptr);
        s_hStaticPlayerName = CreateWindowW(L"STATIC", L"玩家名称:", WS_CHILD | WS_VISIBLE, 40, 156, 70, 24, hwnd, nullptr, nullptr, nullptr);
        s_hEditPlayerName = CreateWindowW(L"EDIT", L"Player1",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 120, 154, 180, 24, hwnd, nullptr, nullptr, nullptr);

        // NAT Traversal settings section
        CreateWindowW(L"BUTTON", L"NAT 穿透设置",
            WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 20, 184, 280, 90, hwnd, nullptr, nullptr, nullptr);
        s_hCheckNatEnabled = CreateWindowW(L"BUTTON", L"启用 NAT 穿透 (UPnP/STUN)",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 30, 202, 260, 24, hwnd, nullptr, nullptr, nullptr);
        s_hCheckNatUpnp = CreateWindowW(L"BUTTON", L"使用 UPnP 端口转发",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 40, 224, 150, 24, hwnd, nullptr, nullptr, nullptr);
        s_hCheckNatStun = CreateWindowW(L"BUTTON", L"使用 STUN 作为后备",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 40, 246, 150, 24, hwnd, nullptr, nullptr, nullptr);
        s_hStaticStunServer = CreateWindowW(L"STATIC", L"STUN 服务器:", WS_CHILD | WS_VISIBLE, 200, 248, 70, 24, hwnd, nullptr, nullptr, nullptr);
        s_hEditStunServer = CreateWindowW(L"EDIT", L"stun.l.google.com:19302",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 270, 246, 120, 24, hwnd, nullptr, nullptr, nullptr);

        CreateWindowW(L"BUTTON", L"启动", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 20, 284, 100, 32, hwnd, (HMENU)1, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"打开配置目录", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 130, 284, 120, 32, hwnd, (HMENU)2, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 260, 284, 60, 32, hwnd, (HMENU)3, nullptr, nullptr);

        LoadToControls();
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == 10 || LOWORD(wp) == 11) {
            UpdateHostIPDisplay();
            return 0;
        }
        if (LOWORD(wp) == 1) {
            SaveFromControls();
            s_launch = true;
            DestroyWindow(hwnd);
            return 0;
        }
        if (LOWORD(wp) == 2) {
            std::wstring wdir;
            ToWide(KenshiMP::GetLauncherDirectory() + "\\config", wdir);
            ShellExecuteW(nullptr, L"explore", wdir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            return 0;
        }
        if (LOWORD(wp) == 3) {
            s_launch = false;
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_CLOSE:
        s_launch = false;
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

bool ShowLauncherDialog(LauncherConfig& config) {
    s_config = &config;
    s_launch = false;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"KenshiMPLauncher";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassExW(&wc);

    s_hMain = CreateWindowExW(0, L"KenshiMPLauncher", L"Kenshi 联机启动器",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        100, 100, 380, 370, nullptr, nullptr, wc.hInstance, nullptr);

    ShowWindow(s_hMain, SW_SHOW);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) && IsWindow(s_hMain)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    s_config = nullptr;
    s_hMain = nullptr;
    return s_launch;
}

} // namespace KenshiMP
