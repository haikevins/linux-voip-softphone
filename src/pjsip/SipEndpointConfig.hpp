#pragma once

#include <cstdint>
#include <string_view>

#include <pjsua2.hpp>

namespace voip::pjsip {

enum class SipTransport
{
    Udp,
    Tcp,
};

[[nodiscard]] std::string_view to_string(SipTransport transport) noexcept;
[[nodiscard]] pjsip_transport_type_e to_pjsip_transport_type(SipTransport transport) noexcept;

struct SipEndpointConfig
{
    SipTransport transport{SipTransport::Udp};
    std::uint16_t listen_port{0U};

    [[nodiscard]] pj::TransportConfig to_pjsua2_transport_config() const;
};

} // namespace voip::pjsip
