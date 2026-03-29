#include "local_ip.h"
#include <winsock2.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>
#include <cstring>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

namespace KenshiMP {

std::vector<std::string> GetLocalIPv4Addresses() {
    std::vector<std::string> out;
    ULONG buf_len = 15000;
    std::vector<BYTE> buf(buf_len);
    PIP_ADAPTER_ADDRESSES p = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buf.data());

    ULONG ret = GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST, nullptr, p, &buf_len);
    if (ret == ERROR_BUFFER_OVERFLOW) {
        buf.resize(buf_len);
        p = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buf.data());
        ret = GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST, nullptr, p, &buf_len);
    }
    if (ret != NO_ERROR) return out;

    for (PIP_ADAPTER_ADDRESSES a = p; a; a = a->Next) {
        if (a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        for (PIP_ADAPTER_UNICAST_ADDRESS u = a->FirstUnicastAddress; u; u = u->Next) {
            if (u->Address.lpSockaddr->sa_family != AF_INET) continue;
            auto* sa = reinterpret_cast<sockaddr_in*>(u->Address.lpSockaddr);
            char ip[32] = {};
            if (InetNtopA(AF_INET, &sa->sin_addr, ip, sizeof(ip))) {
                std::string s(ip);
                if (s != "0.0.0.0") out.push_back(s);
            }
        }
    }
    return out;
}

std::string GetDisplayLocalIP() {
    auto ips = GetLocalIPv4Addresses();
    for (const auto& ip : ips) {
        if (ip.find("192.168.") == 0 || ip.find("10.") == 0 || ip.find("172.") == 0)
            return ip;
    }
    return ips.empty() ? "127.0.0.1" : ips[0];
}

} // namespace KenshiMP
