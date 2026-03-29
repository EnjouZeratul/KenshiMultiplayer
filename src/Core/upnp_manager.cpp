#include "upnp_manager.h"
#include <miniupnpc.h>
#include <upnpcommands.h>
#include <upnpdev.h>
#include <ws2tcpip.h>
#include <cstring>
#include <cstdio>

#pragma comment(lib, "Ws2_32.lib")

namespace KenshiMP {

class UPnPManager::Impl {
public:
    UPNPDev* device_list = nullptr;
    UPNPUrls urls = {};
    IGDdatas data = {};
    bool initialized = false;
    char local_lan_addr[64] = {};

    Impl() {
        memset(&urls, 0, sizeof(urls));
        memset(&data, 0, sizeof(data));
        memset(local_lan_addr, 0, sizeof(local_lan_addr));
    }

    bool Initialize() {
        int error = 0;
        UPNPDev* devlist = upnpDiscover(2000, nullptr, nullptr, 0, 0, 2, &error);

        if (!devlist) {
            return false;
        }

        device_list = devlist;

        // New API requires lanaddr and wanaddr buffers
        char lanaddr[64] = {};
        char wanaddr[64] = {};

        int result = UPNP_GetValidIGD(devlist, &urls, &data,
                                      lanaddr, sizeof(lanaddr),
                                      wanaddr, sizeof(wanaddr));

        if (result == UPNP_NO_IGD || urls.controlURL == nullptr || urls.controlURL[0] == '\0') {
            freeUPNPDevlist(device_list);
            device_list = nullptr;
            return false;
        }

        // Store local LAN address
        strncpy(local_lan_addr, lanaddr, sizeof(local_lan_addr) - 1);

        initialized = true;
        return true;
    }

    void Shutdown() {
        if (device_list) {
            freeUPNPDevlist(device_list);
            device_list = nullptr;
        }
        FreeUPNPUrls(&urls);
        initialized = false;
    }

    bool AddPortMapping(uint16_t external_port, uint16_t internal_port,
                       const std::string& protocol, unsigned int lease_duration,
                       const std::string& description) {
        if (!initialized || !device_list || !urls.controlURL) return false;

        const char* proto = (protocol == "TCP") ? "TCP" : "UDP";

        // Use stored LAN address or fallback
        const char* client = local_lan_addr[0] != '\0' ? local_lan_addr : "0.0.0.0";

        // Convert port numbers to strings
        char ext_port_str[16], int_port_str[16], lease_str[16];
        snprintf(ext_port_str, sizeof(ext_port_str), "%u", external_port);
        snprintf(int_port_str, sizeof(int_port_str), "%u", internal_port);
        snprintf(lease_str, sizeof(lease_str), "%u", lease_duration);

        // UPNP_AddPortMapping has 9 parameters
        int result = UPNP_AddPortMapping(
            urls.controlURL,                      // controlURL
            urls.controlURL_CIF != nullptr ? urls.controlURL_CIF : urls.controlURL,  // servicetype
            ext_port_str,                         // extPort
            int_port_str,                         // inPort
            client,                               // inClient
            description.c_str(),                  // desc
            proto,                                // proto
            nullptr,                              // remoteHost
            lease_str                             // leaseDuration
        );

        return (result == UPNPCOMMAND_SUCCESS);
    }

    bool DeletePortMapping(uint16_t external_port, const std::string& protocol) {
        if (!initialized || !device_list || !urls.controlURL) return false;

        const char* proto = (protocol == "TCP") ? "TCP" : "UDP";

        // Convert port number to string
        char ext_port_str[16];
        snprintf(ext_port_str, sizeof(ext_port_str), "%u", external_port);

        // UPNP_DeletePortMapping has 5 parameters
        int result = UPNP_DeletePortMapping(
            urls.controlURL,                      // controlURL
            urls.controlURL_CIF != nullptr ? urls.controlURL_CIF : urls.controlURL,  // servicetype
            ext_port_str,                         // extPort
            proto,                                // proto
            nullptr                               // remoteHost
        );

        return (result == UPNPCOMMAND_SUCCESS);
    }

    std::string GetExternalIPAddress() {
        if (!initialized || !urls.controlURL) return "";

        char externalIP[64] = {};
        int result = UPNP_GetExternalIPAddress(
            urls.controlURL,
            urls.controlURL_CIF != nullptr ? urls.controlURL_CIF : urls.controlURL,
            externalIP
        );

        if (result == UPNPCOMMAND_SUCCESS && externalIP[0] != '\0') {
            return externalIP;
        }
        return "";
    }

    bool IsAvailable() {
        return initialized && device_list != nullptr && urls.controlURL != nullptr;
    }

    std::string GetStatusMessage() const {
        if (!initialized || !device_list || !urls.controlURL) {
            return "UPnP unavailable - router may not support UPnP";
        }

        std::string ip = const_cast<Impl*>(this)->GetExternalIPAddress();
        if (!ip.empty()) {
            return "UPnP active - detected public IP: " + ip;
        }
        return "UPnP active, but unable to get external IP";
    }
};

// ============================================
// Public API Implementation
// ============================================

UPnPManager::UPnPManager() : impl_(new Impl()) {}

UPnPManager::~UPnPManager() {
    Shutdown();
    delete impl_;
}

bool UPnPManager::Initialize() {
    return impl_->Initialize();
}

void UPnPManager::Shutdown() {
    impl_->Shutdown();
}

bool UPnPManager::AddPortMapping(uint16_t external_port, uint16_t internal_port,
                                 const std::string& protocol,
                                 unsigned int lease_duration,
                                 const std::string& description) {
    return impl_->AddPortMapping(external_port, internal_port, protocol,
                                 lease_duration, description);
}

bool UPnPManager::DeletePortMapping(uint16_t external_port, const std::string& protocol) {
    return impl_->DeletePortMapping(external_port, protocol);
}

std::string UPnPManager::GetExternalIPAddress() const {
    return impl_->GetExternalIPAddress();
}

bool UPnPManager::IsAvailable() const {
    return impl_->IsAvailable();
}

std::string UPnPManager::GetStatusMessage() const {
    return impl_->GetStatusMessage();
}

} // namespace KenshiMP
