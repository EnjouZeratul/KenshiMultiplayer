#pragma once

#include <string>
#include <cstdint>

namespace KenshiMP {

struct InjectResult {
    bool success;
    std::string error_message;
    uint32_t process_id;
};

/// Injects KenshiMP_Core.dll into the target process using CreateRemoteThread + LoadLibrary
InjectResult InjectDLL(uint32_t process_id, const std::string& dll_path);

/// Finds kenshi_x64.exe process by name
uint32_t FindKenshiProcess();

} // namespace KenshiMP
