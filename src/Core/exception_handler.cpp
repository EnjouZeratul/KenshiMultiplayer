#include "exception_handler.h"
#include "logger.h"
#include <Windows.h>
#include <DbgHelp.h>
#include <filesystem>
#include <sstream>

#pragma comment(lib, "dbghelp.lib")

namespace KenshiMP {

static LPTOP_LEVEL_EXCEPTION_FILTER s_prev_filter = nullptr;

static LONG WINAPI ExceptionFilter(EXCEPTION_POINTERS* p) {
    std::stringstream ss;
    ss << "KenshiMP crash: code=0x" << std::hex << p->ExceptionRecord->ExceptionCode
       << " addr=0x" << p->ExceptionRecord->ExceptionAddress;

    Logger::Error(ss.str());

    // Write minidump if possible
    wchar_t dll_path[MAX_PATH] = {};
    HMODULE hSelf = nullptr;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&ExceptionFilter), &hSelf) && hSelf) {
        GetModuleFileNameW(hSelf, dll_path, MAX_PATH);
    }
    std::filesystem::path p_path(dll_path);
    auto dump_path = p_path.parent_path() / "logs" / "kenshi_mp_crash.dmp";
    std::filesystem::create_directories(dump_path.parent_path());

    HANDLE hFile = CreateFileW(dump_path.wstring().c_str(), GENERIC_WRITE, 0, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION mei = {};
        mei.ThreadId = GetCurrentThreadId();
        mei.ExceptionPointers = p;
        mei.ClientPointers = TRUE;

        if (MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile,
                MiniDumpWithThreadInfo, &mei, nullptr, nullptr)) {
            Logger::Info("Minidump written to " + dump_path.string());
        }
        CloseHandle(hFile);
    }

    if (s_prev_filter)
        return s_prev_filter(p);
    return EXCEPTION_CONTINUE_SEARCH;
}

void InstallExceptionHandler() {
    s_prev_filter = SetUnhandledExceptionFilter(ExceptionFilter);
}

void UninstallExceptionHandler() {
    SetUnhandledExceptionFilter(s_prev_filter);
    s_prev_filter = nullptr;
}

} // namespace KenshiMP
