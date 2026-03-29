#include "nat_traversal.h"
#include "upnp_manager.h"
#include "stun_manager.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <chrono>
#include <algorithm>
#include <random>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")

namespace KenshiMP {

// Helper function to get local IP addresses
std::vector<std::string> GetLocalIPv4Addresses();

class NatTraversal::Impl {
public:
    std::unique_ptr<UPnPManager> upnp_mgr;
    std::unique_ptr<STUNManager> stun_mgr;

    std::vector<std::string> custom_stun_servers;
    bool upnp_enabled = true;
    bool stun_enabled = true;

    ConnectionAddress addresses{};
    NatTraversalResult result = NatTraversalResult::Unknown;

    std::atomic<bool> initialized{false};
    std::atomic<bool> shutdown_requested{false};

    NatProgressCallback progress_cb;

    void EmitProgress(const std::string& msg) {
        if (progress_cb) {
            progress_cb(msg);
        }
    }

    std::vector<std::string> GetLocalIPs() {
        std::vector<std::string> ips;

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

        if (ret != NO_ERROR) return ips;

        for (PIP_ADAPTER_ADDRESSES a = p; a; a = a->Next) {
            if (a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
            for (PIP_ADAPTER_UNICAST_ADDRESS u = a->FirstUnicastAddress; u; u = u->Next) {
                if (u->Address.lpSockaddr->sa_family != AF_INET) continue;
                auto* sa = reinterpret_cast<sockaddr_in*>(u->Address.lpSockaddr);
                char ip[32] = {};
                if (InetNtopA(AF_INET, &sa->sin_addr, ip, sizeof(ip))) {
                    std::string s(ip);
                    if (s != "0.0.0.0") ips.push_back(s);
                }
            }
        }
        return ips;
    }

    bool Initialize(bool enable_upnp, bool enable_stun, NatProgressCallback callback) {
        upnp_enabled = enable_upnp;
        stun_enabled = enable_stun;
        progress_cb = callback;

        EmitProgress("正在初始化 NAT 穿透模块...");

        // Initialize local IP addresses first
        auto local_ips = GetLocalIPs();
        if (local_ips.empty()) {
            addresses.local_ip = "127.0.0.1";
            addresses.local_port = 0;
            addresses.is_public_reachable = false;
            result = NatTraversalResult::Unknown;
            EmitProgress("警告：未检测到本地网络接口");
            return false;
        }

        // Use first non-loopback IP as local
        for (const auto& ip : local_ips) {
            if (ip != "127.0.0.1") {
                addresses.local_ip = ip;
                break;
            }
        }
        if (addresses.local_ip.empty()) {
            addresses.local_ip = local_ips[0];
        }

        // Try UPnP first if enabled
        if (upnp_enabled) {
            EmitProgress("正在检测 UPnP 设备...");
            upnp_mgr = std::make_unique<UPnPManager>();

            if (upnp_mgr->Initialize()) {
                EmitProgress("UPnP 设备已发现，尝试端口映射...");

                std::string external_ip = upnp_mgr->GetExternalIPAddress();
                if (!external_ip.empty()) {
                    addresses.public_ip = external_ip;
                    addresses.is_public_reachable = true;
                    result = NatTraversalResult::SuccessUPnP;
                    EmitProgress("UPnP 成功！公网 IP: " + external_ip);
                    return true;
                } else {
                    EmitProgress("UPnP 初始化成功但无法获取外部 IP");
                    result = NatTraversalResult::UpnpFailed;
                }
            } else {
                EmitProgress("未找到 UPnP 设备");
                result = NatTraversalResult::UpnpFailed;
            }
        }

        // Try STUN as fallback
        if (stun_enabled) {
            EmitProgress("正在连接 STUN 服务器...");
            upnp_mgr.reset();
            stun_mgr = std::make_unique<STUNManager>();

            auto servers = custom_stun_servers.empty()
                ? STUNManager::GetDefaultServers()
                : custom_stun_servers;

            std::string stun_result = stun_mgr->QueryMultipleServers(servers, 5000);

            if (!stun_result.empty()) {
                // Parse "ip:port" format
                auto pos = stun_result.rfind(':');
                if (pos != std::string::npos) {
                    addresses.public_ip = stun_result.substr(0, pos);
                    try {
                        addresses.public_port = static_cast<uint16_t>(
                            std::stoi(stun_result.substr(pos + 1)));
                    } catch (...) {
                        addresses.public_port = 0;
                    }

                    // Determine reachability based on NAT type
                    auto nat_type = stun_mgr->DetectNATType();
                    addresses.is_public_reachable = (nat_type != STUNManager::NatType::Symmetric);

                    if (addresses.is_public_reachable) {
                        result = NatTraversalResult::SuccessSTUN;
                        EmitProgress("STUN 成功！可以进行 P2P 连接");
                    } else {
                        result = NatTraversalResult::StunFailed;
                        EmitProgress("STUN 返回但存在对称 NAT，仅局域网可用");
                    }

                    return addresses.is_public_reachable;
                }
            } else {
                EmitProgress("STUN 服务器连接失败");
                result = NatTraversalResult::StunFailed;
            }
        }

        // Fall back to local only
        result = NatTraversalResult::SuccessLocalOnly;
        addresses.is_public_reachable = false;
        EmitProgress("仅局域网模式 - 无法发现公网地址");
        return false;
    }

    void Shutdown() {
        shutdown_requested = true;
        if (upnp_mgr) {
            upnp_mgr->Shutdown();
            upnp_mgr.reset();
        }
        if (stun_mgr) {
            stun_mgr.reset();
        }
        result = NatTraversalResult::Unknown;
        initialized = false;
    }
};

// ============================================
// Public API Implementation
// ============================================

NatTraversal::NatTraversal() : impl_(new Impl()) {}

NatTraversal::~NatTraversal() {
    Shutdown();
    delete impl_;
}

bool NatTraversal::Initialize(bool enable_upnp, bool enable_stun,
                             NatProgressCallback progress_callback) {
    impl_->initialized = true;
    return impl_->Initialize(enable_upnp, enable_stun, progress_callback);
}

void NatTraversal::Shutdown() {
    if (impl_) {
        impl_->Shutdown();
    }
}

void NatTraversal::SetCustomStunServers(const std::vector<std::string>& servers) {
    if (impl_) {
        impl_->custom_stun_servers = servers;
    }
}

void NatTraversal::ResetToDefaultStunServers() {
    if (impl_) {
        impl_->custom_stun_servers.clear();
    }
}

bool NatTraversal::GetExternalAddress(std::string& ip, uint16_t& port) {
    if (!impl_) return false;

    ip = impl_->addresses.public_ip;
    port = impl_->addresses.public_port;

    return !ip.empty() && impl_->addresses.is_public_reachable;
}

ConnectionAddress NatTraversal::GetConnectionAddresses() const {
    if (!impl_) {
        return ConnectionAddress{};
    }
    return impl_->addresses;
}

NatTraversalResult NatTraversal::GetResult() const {
    return impl_ ? impl_->result : NatTraversalResult::Unknown;
}

bool NatTraversal::IsInternetAccessible() const {
    return impl_ ? impl_->addresses.is_public_reachable : false;
}

bool NatTraversal::IsLANAccessible() const {
    return impl_ && !impl_->addresses.local_ip.empty();
}

std::string NatTraversal::GetStatusMessage() const {
    if (!impl_) return "未初始化";

    switch (impl_->result) {
        case NatTraversalResult::SuccessUPnP:
            return "互联网连接：可通过公网 IP 连接 (" + impl_->addresses.public_ip + ")"
                   "\n局域网连接：可通过本地 IP 连接 (" + impl_->addresses.local_ip + ")";

        case NatTraversalResult::SuccessSTUN:
            if (impl_->addresses.is_public_reachable) {
                return "互联网连接：STUN 穿透成功，可接受外部连接 (" + impl_->addresses.public_ip + ")"
                       "\n局域网连接：支持本地连接 (" + impl_->addresses.local_ip + ")";
            } else {
                return "互联网连接：对称 NAT，仅主机可发起连接"
                       "\n局域网连接：支持本地连接 (" + impl_->addresses.local_ip + ")";
            }

        case NatTraversalResult::SuccessLocalOnly:
            return "仅局域网模式：只接受本地网络内连接"
                   "\n本地 IP: " + impl_->addresses.local_ip;

        case NatTraversalResult::UnsupportedNAT:
            return "不支持的 NAT 类型，仅局域网可用";

        case NatTraversalResult::UpnpFailed:
            if (impl_->stun_enabled) {
                return "UPnP 失败，尝试 STUN 方法";
            }
            return "UPnP 失败，仅局域网模式";

        case NatTraversalResult::StunFailed:
            return "所有 NAT 穿透方法失败，仅局域网模式"
                   "\n本地 IP: " + impl_->addresses.local_ip;

        case NatTraversalResult::Unknown:
        default:
            return "状态未知 - 正在初始化";
    }
}

} // namespace KenshiMP
