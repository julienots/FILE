#include "agr/net/Upnp.hpp"

#include <chrono>
#include <thread>

#include "agr/net/Http.hpp"
#include "agr/net/Sockets.hpp"

namespace agr::net {

namespace {

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

/// Value of the first <tag>...</tag> (namespace prefixes ignored), "" if absent.
std::string tagValue(const std::string& xml, const std::string& tag, usize from = 0, usize to = std::string::npos) {
    const std::string lx = lower(xml.substr(0, to == std::string::npos ? xml.size() : to));
    const std::string want = lower(tag);
    for (usize at = lx.find('<', from); at != std::string::npos; at = lx.find('<', at + 1)) {
        usize nameStart = at + 1;
        if (nameStart < lx.size() && lx[nameStart] == '/') continue;
        const usize nameEnd = lx.find_first_of("> \t\r\n", nameStart);
        if (nameEnd == std::string::npos) break;
        std::string name = lx.substr(nameStart, nameEnd - nameStart);
        if (const usize colon = name.find(':'); colon != std::string::npos) name = name.substr(colon + 1);
        if (name != want) continue;
        const usize open = lx.find('>', nameEnd);
        if (open == std::string::npos) break;
        const usize close = lx.find("</", open);
        if (close == std::string::npos) break;
        std::string v = xml.substr(open + 1, close - open - 1);
        while (!v.empty() && std::isspace(static_cast<unsigned char>(v.front()))) v.erase(0, 1);
        while (!v.empty() && std::isspace(static_cast<unsigned char>(v.back()))) v.pop_back();
        return v;
    }
    return {};
}

std::string headerValue(const std::string& response, const std::string& name) {
    const std::string l = lower(response), want = "\n" + lower(name) + ":";
    const usize at = l.find(want);
    if (at == std::string::npos) return {};
    const usize start = at + want.size(), end = response.find("\r\n", start);
    std::string v = response.substr(start, end == std::string::npos ? std::string::npos : end - start);
    while (!v.empty() && v.front() == ' ') v.erase(0, 1);
    return v;
}

std::string xmlEscape(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '<') out += "&lt;";
        else if (c == '>') out += "&gt;";
        else if (c == '&') out += "&amp;";
        else out += c;
    }
    return out;
}

} // namespace

Result<UpnpGateway> UpnpGateway::discover(f64 timeoutSeconds, const std::string& ssdpAddress, u16 ssdpPort) {
    auto udp = UdpSocket::open(0);
    if (!udp) return udp.error();
    const char* targets[] = {"urn:schemas-upnp-org:device:InternetGatewayDevice:1", "urn:schemas-upnp-org:device:InternetGatewayDevice:2",
                             "urn:schemas-upnp-org:service:WANIPConnection:1"};
    for (const char* st : targets) {
        const std::string msg = "M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\nMAN: \"ssdp:discover\"\r\nMX: 2\r\nST: " + std::string(st) + "\r\n\r\n";
        udp.value()->sendTo(ssdpAddress, ssdpPort, msg);
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::duration<double>(timeoutSeconds);
    std::string location;
    while (location.empty() && std::chrono::steady_clock::now() < deadline) {
        for (const auto& d : udp.value()->receive()) {
            const std::string loc = headerValue(d.data, "location");
            if (!loc.empty()) {
                location = loc;
                break;
            }
        }
        if (location.empty()) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (location.empty()) return makeError(Error::Code::NotFound, "no UPnP router answered (UPnP off on the router, or no router)");

    HttpRequest req;
    req.url = location;
    req.direct = true;
    req.timeoutSeconds = 5.0;
    const HttpResponse desc = httpRequest(req);
    if (!desc.ok()) return makeError(Error::Code::IOError, "router description: " + (desc.error.empty() ? std::to_string(desc.status) : desc.error));
    // The WAN connection service (IP or PPP) and its control URL.
    UpnpGateway g;
    const std::string& xml = desc.body;
    const std::string lx = lower(xml);
    for (const char* type : {"urn:schemas-upnp-org:service:wanipconnection:", "urn:schemas-upnp-org:service:wanpppconnection:"}) {
        const usize at = lx.find(type);
        if (at == std::string::npos) continue;
        const usize blockStart = lx.rfind("<service>", at), blockEnd = lx.find("</service>", at);
        if (blockStart == std::string::npos || blockEnd == std::string::npos) continue;
        g.m_serviceType = tagValue(xml, "serviceType", blockStart, blockEnd);
        std::string control = tagValue(xml, "controlURL", blockStart, blockEnd);
        if (control.rfind("http", 0) != 0) {  // relative: to URLBase, else to the description's host
            std::string base = tagValue(xml, "URLBase");
            if (base.empty()) {
                auto u = Url::parse(location);
                if (!u) return u.error();
                base = u.value().scheme + "://" + u.value().host + ":" + std::to_string(u.value().port);
            }
            while (!base.empty() && base.back() == '/') base.pop_back();
            control = base + (control.empty() || control[0] != '/' ? "/" : "") + control;
        }
        g.m_controlUrl = control;
        break;
    }
    if (g.m_controlUrl.empty()) return makeError(Error::Code::Unsupported, "the router has no WAN connection service");
    if (auto u = Url::parse(g.m_controlUrl)) g.m_localAddress = localAddressFor(u.value().host);
    return g;
}

Result<std::string> UpnpGateway::soap(const std::string& action, const std::string& arguments) const {
    HttpRequest req;
    req.method = "POST";
    req.url = m_controlUrl;
    req.direct = true;
    req.timeoutSeconds = 5.0;
    req.headers = {{"Content-Type", "text/xml; charset=\"utf-8\""}, {"SOAPAction", "\"" + m_serviceType + "#" + action + "\""}};
    req.body = "<?xml version=\"1.0\"?>\r\n<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
               "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body><u:" +
               action + " xmlns:u=\"" + m_serviceType + "\">" + arguments + "</u:" + action + "></s:Body></s:Envelope>\r\n";
    const HttpResponse r = httpRequest(req);
    if (!r.error.empty()) return makeError(Error::Code::IOError, "UPnP " + action + ": " + r.error);
    if (r.status != 200) {
        const std::string why = tagValue(r.body, "errorDescription");
        return makeError(Error::Code::Unknown, "UPnP " + action + " refused (" + std::to_string(r.status) + (why.empty() ? "" : ", " + why) + ")");
    }
    return r.body;
}

Result<std::string> UpnpGateway::externalAddress() const {
    auto r = soap("GetExternalIPAddress", "");
    if (!r) return r.error();
    std::string ip = tagValue(r.value(), "NewExternalIPAddress");
    if (ip.empty()) return makeError(Error::Code::NotFound, "the router did not give its public address");
    return ip;
}

Status UpnpGateway::addPortMapping(u16 externalPort, u16 internalPort, const std::string& protocol, const std::string& description, u32 leaseSeconds) const {
    if (m_localAddress.empty()) return makeError(Error::Code::NotFound, "local address unknown");
    auto r = soap("AddPortMapping", "<NewRemoteHost></NewRemoteHost><NewExternalPort>" + std::to_string(externalPort) + "</NewExternalPort><NewProtocol>" +
                                        protocol + "</NewProtocol><NewInternalPort>" + std::to_string(internalPort) + "</NewInternalPort><NewInternalClient>" +
                                        m_localAddress + "</NewInternalClient><NewEnabled>1</NewEnabled><NewPortMappingDescription>" +
                                        xmlEscape(description) + "</NewPortMappingDescription><NewLeaseDuration>" + std::to_string(leaseSeconds) +
                                        "</NewLeaseDuration>");
    if (!r) return r.error();
    return {};
}

Status UpnpGateway::deletePortMapping(u16 externalPort, const std::string& protocol) const {
    auto r = soap("DeletePortMapping", "<NewRemoteHost></NewRemoteHost><NewExternalPort>" + std::to_string(externalPort) + "</NewExternalPort><NewProtocol>" +
                                           protocol + "</NewProtocol>");
    if (!r) return r.error();
    return {};
}

} // namespace agr::net
