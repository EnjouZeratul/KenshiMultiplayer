#include "stun_manager.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstring>
#include <chrono>
#include <thread>
#include <vector>
#include <algorithm>

#pragma comment(lib, "Ws2_32.lib")

namespace KenshiMP {

// STUN message structure (RFC 5389)
#pragma pack(push, 1)
struct StunMessage {
    uint16_t type;
    uint16_t length;
    uint32_t magic_cookie;
    uint8_t transaction_id[12];
};
#pragma pack(pop)

class STUNManager::Impl {
public:
    SOCKET stun_socket = INVALID_SOCKET;
    bool initialized = false;

    bool Initialize() {
        WSADATA wsa_data;
        if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
            return false;
        }

        stun_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (stun_socket == INVALID_SOCKET) {
            WSACleanup();
            return false;
        }

        // Set receive timeout
        DWORD timeout = 5000;
        setsockopt(stun_socket, SOL_SOCKET, SO_RCVTIMEO,
                  reinterpret_cast<const char*>(&timeout), sizeof(timeout));

        initialized = true;
        return true;
    }

    void Shutdown() {
        if (stun_socket != INVALID_SOCKET) {
            closesocket(stun_socket);
            stun_socket = INVALID_SOCKET;
        }
        WSACleanup();
        initialized = false;
    }

    sockaddr_in ParseServerAddress(const std::string& server) {
        sockaddr_in addr{};
        addr.sin_family = AF_INET;

        size_t colon_pos = server.rfind(':');
        if (colon_pos == std::string::npos) {
            return addr;
        }

        std::string ip = server.substr(0, colon_pos);
        int port = std::stoi(server.substr(colon_pos + 1));

        addr.sin_port = htons(port);
        inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

        return addr;
    }

    bool SendBindingRequest(const sockaddr_in& server_addr) {
        StunMessage msg{};
        msg.type = htons(0x0001);  // Binding Request
        msg.length = 0;
        msg.magic_cookie = htonl(0x2112A442);

        // Generate random transaction ID
        for (int i = 0; i < 12; i++) {
            msg.transaction_id[i] = static_cast<uint8_t>(rand() % 256);
        }

        int sent = sendto(stun_socket, reinterpret_cast<const char*>(&msg),
                         sizeof(msg), 0,
                         reinterpret_cast<const sockaddr*>(&server_addr),
                         sizeof(server_addr));

        return (sent == sizeof(msg));
    }

    std::string ReceiveBindingResponse() {
        char buffer[512] = {};
        sockaddr_in from_addr{};
        int from_len = sizeof(from_addr);

        int received = recvfrom(stun_socket, buffer, sizeof(buffer), 0,
                               reinterpret_cast<sockaddr*>(&from_addr), &from_len);

        if (received < static_cast<int>(sizeof(StunMessage))) {
            return "";
        }

        StunMessage* response = reinterpret_cast<StunMessage*>(buffer);
        uint16_t msg_type = ntohs(response->type);

        // Check for error response
        if (msg_type == 0x0101) {
            return "";
        }

        // Check for successful binding response
        if (msg_type != 0x0102) {
            return "";
        }

        // Parse XOR-MAPPED-ADDRESS attribute (type 0x0020)
        uint16_t attr_count = ntohs(response->length);
        const char* ptr = buffer + sizeof(StunMessage);
        const char* end = ptr + attr_count;

        while (ptr + 4 <= end) {
            uint16_t attr_type = ntohs(*reinterpret_cast<const uint16_t*>(ptr));
            uint16_t attr_len = ntohs(*reinterpret_cast<const uint16_t*>(ptr + 2));
            ptr += 4;

            if (attr_type == 0x0020 && attr_len >= 8) {
                // XOR-MAPPED-ADDRESS
                uint8_t family = reinterpret_cast<const uint8_t*>(ptr)[1];
                if (family == 0x01) {  // IPv4
                    uint16_t xored_port = ntohs(*reinterpret_cast<const uint16_t*>(ptr + 2));
                    uint32_t xored_ip = ntohl(*reinterpret_cast<const uint32_t*>(ptr + 4));

                    // XOR with magic cookie and transaction ID
                    uint16_t real_port = xored_port ^ (ntohs(response->magic_cookie) & 0xFFFF);

                    uint32_t xor_key = response->magic_cookie;
                    uint32_t real_ip = xored_ip ^ xor_key;

                    char ip_str[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &real_ip, ip_str, INET_ADDRSTRLEN);

                    char result[64];
                    snprintf(result, sizeof(result), "%s:%u", ip_str, real_port);
                    return std::string(result);
                }
            }

            // Move to next attribute (4-byte aligned)
            ptr += ((attr_len + 3) / 4) * 4;
        }

        return "";
    }

    std::string QueryServer(const std::string& stun_server, int timeout_ms) {
        if (!initialized || stun_socket == INVALID_SOCKET) {
            if (!Initialize()) {
                return "";
            }
        }

        sockaddr_in server_addr = ParseServerAddress(stun_server);
        if (server_addr.sin_family != AF_INET) {
            return "";
        }

        // Set timeout
        DWORD timeout = static_cast<DWORD>(timeout_ms);
        setsockopt(stun_socket, SOL_SOCKET, SO_RCVTIMEO,
                  reinterpret_cast<const char*>(&timeout), sizeof(timeout));

        if (!SendBindingRequest(server_addr)) {
            return "";
        }

        return ReceiveBindingResponse();
    }

    NatType DetectNATType() {
        // Simplified NAT type detection
        // Full implementation would require multiple test servers
        // For now, assume compatible NAT if we got a response
        return NatType::FullCone;
    }
};

// ============================================
// Public API Implementation
// ============================================

const std::vector<std::string>& STUNManager::GetDefaultServers() {
    static const std::vector<std::string> servers = {
        "stun.l.google.com:19302",
        "stun1.l.google.com:19302",
        "stun2.l.google.com:19302",
        "stun3.l.google.com:19302",
        "stun4.l.google.com:19302",
        "stun.stunprotocol.org:3478",
        "stun.voip.blackberry.com:3478",
        "223.5.5.5:3478",  // Aliyun public STUN (China)
        "180.76.76.76:3478"  // Baidu public STUN (China)
    };
    return servers;
}

STUNManager::STUNManager() : impl_(new Impl()) {}

STUNManager::~STUNManager() {
    delete impl_;
}

bool STUNManager::IsInitialized() const {
    return impl_->initialized;
}

std::string STUNManager::QueryServer(const std::string& stun_server, int timeout_ms) {
    return impl_->QueryServer(stun_server, timeout_ms);
}

std::string STUNManager::QueryMultipleServers(const std::vector<std::string>& servers,
                                               int timeout_ms) {
    std::string result;

    for (size_t i = 0; i < servers.size(); ++i) {
        int current_timeout = timeout_ms / static_cast<int>(i + 1);
        // Use ternary operator instead of std::max to avoid Windows max macro conflict
        current_timeout = (current_timeout > 2000) ? current_timeout : 2000;

        result = impl_->QueryServer(servers[i], current_timeout);
        if (!result.empty()) {
            break;
        }
    }

    return result;
}

STUNManager::NatType STUNManager::DetectNATType() {
    return impl_->DetectNATType();
}

} // namespace KenshiMP
