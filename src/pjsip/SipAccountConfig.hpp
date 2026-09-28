#pragma once

#include "pjsip/SipEndpointConfig.hpp"

#include <cstdint>
#include <string>

#include <pjsua2.hpp>

namespace voip::pjsip {

struct SipAccountConfig
{
    std::string username;
    std::string password;
    std::string server;
    std::uint16_t server_port{5060U};
    std::string realm{"*"};
    SipTransport transport{SipTransport::Udp};
    bool register_on_add{true};

    void validate() const;

    [[nodiscard]] std::string identity_uri() const;
    [[nodiscard]] std::string registrar_uri() const;
    [[nodiscard]] pj::AccountConfig to_pjsua2_account_config() const;
};

} // namespace voip::pjsip
