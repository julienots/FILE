#pragma once
// UPnP IGD port forwarding (like Godot's UPNP class): finds the router on the local network
// (SSDP), then asks it to forward a port to this machine, so that players on the Internet can
// join a game hosted at home. Blocking calls: run them off the game thread (agr.upnp does).
#include <string>

#include "agr/core/Result.hpp"
#include "agr/core/Types.hpp"

namespace agr::net {

class UpnpGateway {
public:
    /// Searches the router (M-SEARCH to `ssdpAddress`, the standard multicast group by default) for
    /// up to `timeoutSeconds`, reads its description and finds its WAN connection service.
    static Result<UpnpGateway> discover(f64 timeoutSeconds = 2.0, const std::string& ssdpAddress = "239.255.255.250", u16 ssdpPort = 1900);
    /// The router's public address.
    Result<std::string> externalAddress() const;
    /// Forwards `externalPort` (UDP or TCP) to this machine's `internalPort`; lease 0 = until removed.
    Status addPortMapping(u16 externalPort, u16 internalPort, const std::string& protocol = "UDP", const std::string& description = "AGR game",
                          u32 leaseSeconds = 0) const;
    Status deletePortMapping(u16 externalPort, const std::string& protocol = "UDP") const;
    const std::string& controlUrl() const { return m_controlUrl; }
    const std::string& serviceType() const { return m_serviceType; }
    const std::string& localAddress() const { return m_localAddress; }  ///< This machine, as the router sees it.

private:
    Result<std::string> soap(const std::string& action, const std::string& arguments) const;
    std::string m_controlUrl, m_serviceType, m_localAddress;
};

} // namespace agr::net
