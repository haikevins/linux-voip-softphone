#include "pjsip/SipEndpointConfig.hpp"

namespace voip::pjsip {

std::string_view to_string(const SipTransport transport) noexcept
{
    switch (transport) {
    case SipTransport::Udp:
        return "udp";
    case SipTransport::Tcp:
        return "tcp";
    }

    return "udp";
}

pjsip_transport_type_e to_pjsip_transport_type(const SipTransport transport) noexcept
{
    switch (transport) {
    case SipTransport::Udp:
        return PJSIP_TRANSPORT_UDP;
    case SipTransport::Tcp:
        return PJSIP_TRANSPORT_TCP;
    }

    return PJSIP_TRANSPORT_UDP;
}

pj::TransportConfig SipEndpointConfig::to_pjsua2_transport_config() const
{
    pj::TransportConfig config;
    config.port = listen_port;
    return config;
}

} // namespace voip::pjsip
