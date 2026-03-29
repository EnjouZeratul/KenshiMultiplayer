#pragma once

#include <string>

namespace KenshiMP {

class Logger {
public:
    static void Init();
    static void Shutdown();
    static void Info(const std::string& msg);
    static void Error(const std::string& msg);
    static void Debug(const std::string& msg);
};

} // namespace KenshiMP
