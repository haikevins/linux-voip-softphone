#pragma once

#include "audio/AudioDeviceSelection.hpp"
#include "pjsip/SipAccountConfig.hpp"
#include "pjsip/SipEndpointConfig.hpp"

#include <chrono>
#include <span>
#include <string>
#include <string_view>

namespace voip::pjsip {

struct OutgoingCallCliOptions
{
    SipEndpointConfig endpoint;
    SipAccountConfig account;
    std::string target;
    audio::AudioDeviceSelector capture_device{"default", std::nullopt};
    audio::AudioDeviceSelector playback_device{"default", std::nullopt};
    std::chrono::milliseconds registration_timeout{10'000};
    std::chrono::milliseconds call_timeout{30'000};
    std::chrono::milliseconds hold_duration{2'000};
    std::string password_environment{"VOIP_SIP_PASSWORD"};
    bool show_help{false};

    [[nodiscard]] std::string destination_uri() const;
};

[[nodiscard]] OutgoingCallCliOptions
parse_outgoing_call_cli(std::span<const std::string_view> arguments);
[[nodiscard]] std::string outgoing_call_cli_usage();

} // namespace voip::pjsip
