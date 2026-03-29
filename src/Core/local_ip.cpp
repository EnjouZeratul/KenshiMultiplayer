#include "local_ip.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <vector>
#include <string>
#include <algorithm>

#pragma comment(lib, "Iphlpapi.lib")
#pragma comment(lib, "Ws2_32.lib")

namespace KenshiMP {

std::vector<std::string> GetLocalIPv4Addresses() {
    std::vector<std::string> addresses;

    // Initialize Winsock
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        return addresses;
    }

    // Use GetAdaptersAddresses for accurate information
    ULONG buf_len = 15000;
    std::vector<BYTE> buf(buf_len);
    PIP_ADAPTER_ADDRESSES p = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buf.data());

    ULONG ret = GetAdaptersAddresses(AF_INET,
        GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST,
        nullptr, p, &buf_len);

    if (ret == ERROR_BUFFER_OVERFLOW) {
        buf.resize(buf_len);
        p = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buf.data());
        ret = GetAdaptersAddresses(AF_INET,
            GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST,
            nullptr, p, &buf_len);
    }

    if (ret != NO_ERROR) {
        WSACleanup();
        return addresses;
    }

    // Iterate through all adapter addresses
    for (PIP_ADAPTER_ADDRESSES a = p; a; a = a->Next) {
        // Skip loopback adapters
        if (a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;

        // Iterate through unicast addresses
        for (PIP_ADAPTER_UNICAST_ADDRESS u = a->FirstUnicastAddress; u; u = u->Next) {
            if (u->Address.lpSockaddr->sa_family != AF_INET) continue;

            auto* sa = reinterpret_cast<sockaddr_in*>(u->Address.lpSockaddr);
            char ip_str[INET_ADDRSTRLEN];

            if (inet_ntop(AF_INET, &sa->sin_addr, ip_str, sizeof(ip_str))) {
                std::string ip(ip_str);
                if (ip != "0.0.0.0") {
                    addresses.push_back(ip);
                }
            }
        }
    }

    WSACleanup();
    return addresses;
}

std::string GetDisplayLocalIP() {
    auto ips = GetLocalIPv4Addresses();

    // Prioritize LAN IP ranges (most common for local gaming)
    for (const auto& ip : ips) {
        if (ip.find("192.168.") == 0 ||
            ip.find("10.") == 0 ||
            ip.find("172.16.") == 0 ||
            ip.find("172.17.") == 0 ||
            ip.find("172.18.") == 0 ||
            ip.find("172.19.") == 0 ||
            ip.find("172.2") == 0 ||
            ip.find("172.30.") == 0 ||
            ip.find("172.31.") == 0) {
            return ip;
        }
    }

    // Fall back to any non-loopback address
    for (const auto& ip : ips) {
        if (ip != "127.0.0.1" && ip != "localhost") {
            return ip;
        }
    }

    // Final fallback to loopback
    return ips.empty() ? "127.0.0.1" : ips[0];
}

} // namespace KenshiMP
