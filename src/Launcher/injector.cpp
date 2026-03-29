#include "injector.h"
#include <Windows.h>
#include <TlHelp32.h>
#include <string>
#include <cstring>

namespace KenshiMP {

InjectResult InjectDLL(uint32_t process_id, const std::string& dll_path) {
    InjectResult result = { false, "", process_id };

    HANDLE hProcess = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |
        PROCESS_VM_WRITE | PROCESS_VM_READ,
        FALSE, process_id);

    if (!hProcess) {
        result.error_message = "OpenProcess failed: " + std::to_string(GetLastError());
        return result;
    }

    size_t path_len = dll_path.size() + 1;
    LPVOID remote_mem = VirtualAllocEx(hProcess, nullptr, path_len,
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    if (!remote_mem) {
        result.error_message = "VirtualAllocEx failed: " + std::to_string(GetLastError());
        CloseHandle(hProcess);
        return result;
    }

    if (!WriteProcessMemory(hProcess, remote_mem, dll_path.c_str(), path_len, nullptr)) {
        result.error_message = "WriteProcessMemory failed: " + std::to_string(GetLastError());
        VirtualFreeEx(hProcess, remote_mem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return result;
    }

    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    if (!hKernel32) {
        result.error_message = "GetModuleHandle(kernel32) failed";
        VirtualFreeEx(hProcess, remote_mem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return result;
    }

    FARPROC load_library_addr = GetProcAddress(hKernel32, "LoadLibraryA");
    if (!load_library_addr) {
        result.error_message = "GetProcAddress(LoadLibraryA) failed";
        VirtualFreeEx(hProcess, remote_mem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return result;
    }

    HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0,
        (LPTHREAD_START_ROUTINE)load_library_addr, remote_mem, 0, nullptr);

    if (!hThread) {
        result.error_message = "CreateRemoteThread failed: " + std::to_string(GetLastError());
        VirtualFreeEx(hProcess, remote_mem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return result;
    }

    WaitForSingleObject(hThread, 5000);
    CloseHandle(hThread);
    VirtualFreeEx(hProcess, remote_mem, 0, MEM_RELEASE);
    CloseHandle(hProcess);

    result.success = true;
    return result;
}

uint32_t FindKenshiProcess() {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return 0;

    PROCESSENTRY32W pe = { sizeof(pe) };
    if (Process32FirstW(snapshot, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, L"kenshi_x64.exe") == 0) {
                CloseHandle(snapshot);
                return pe.th32ProcessID;
            }
        } while (Process32NextW(snapshot, &pe));
    }

    CloseHandle(snapshot);
    return 0;
}

} // namespace KenshiMP
