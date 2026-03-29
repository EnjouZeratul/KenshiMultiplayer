#pragma once

#include <string>
#include <vector>

namespace KenshiMP {

/// 获取本机 IPv4 地址列表（用于房主显示）
std::vector<std::string> GetLocalIPv4Addresses();

/// 获取首选显示用的 IP（局域网优先，跳过回环）
std::string GetDisplayLocalIP();

} // namespace KenshiMP
