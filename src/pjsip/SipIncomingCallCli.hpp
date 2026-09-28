#pragma once

#include "pjsip/SipAccountConfig.hpp"
#include "pjsip/SipEndpointConfig.hpp"

#include <chrono>
#include <span>
#include <string>
#include <string_view>

namespace voip::pjsip {

struct IncomingCallCliOptions
{
    SipEndpointConfig endpoint;
    SipAccountConfig account;
    std::chrono::milliseconds registration_timeout{10'000};
    std::chrono::milliseconds incoming_timeout{30'000};
    std::chrono::milliseconds call_timeout{10'000};
    std::chrono::milliseconds answer_delay{0};
    std::chrono::milliseconds hold_duration{2'000};
    std::string password_environment{"VOIP_SIP_PASSWORD"};
    bool show_help{false};
};

[[nodiscard]] IncomingCallCliOptions
parse_incoming_call_cli(std::span<const std::string_view> arguments);
[[nodiscard]] std::string incoming_call_cli_usage();

} // namespace voip::pjsip
