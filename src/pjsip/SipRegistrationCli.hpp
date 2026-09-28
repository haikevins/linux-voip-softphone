#pragma once

#include "pjsip/SipAccountConfig.hpp"
#include "pjsip/SipEndpointConfig.hpp"

#include <chrono>
#include <span>
#include <string>
#include <string_view>

namespace voip::pjsip {

struct RegistrationCliOptions
{
    SipEndpointConfig endpoint;
    SipAccountConfig account;
    std::chrono::milliseconds timeout{10'000};
    std::string password_environment{"VOIP_SIP_PASSWORD"};
    bool show_help{false};
};

[[nodiscard]] RegistrationCliOptions
parse_registration_cli(std::span<const std::string_view> arguments);
[[nodiscard]] std::string registration_cli_usage();

} // namespace voip::pjsip
