#include "pjsip/SipOutgoingCallCli.hpp"

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <string_view>

namespace {

using voip::pjsip::SipTransport;

TEST(SipOutgoingCallCliTest, ParsesRequiredValuesAndDefaults)
{
    constexpr std::array arguments{std::string_view{"--username"}, std::string_view{"1002"},
                                   std::string_view{"--server"},   std::string_view{"192.168.1.13"},
                                   std::string_view{"--target"},   std::string_view{"1001"}};

    const auto options = voip::pjsip::parse_outgoing_call_cli(arguments);

    EXPECT_EQ(options.account.username, "1002");
    EXPECT_EQ(options.account.server, "192.168.1.13");
    EXPECT_EQ(options.target, "1001");
    EXPECT_EQ(options.capture_device.name, "default");
    EXPECT_FALSE(options.capture_device.driver.has_value());
    EXPECT_EQ(options.playback_device.name, "default");
    EXPECT_FALSE(options.playback_device.driver.has_value());
    EXPECT_EQ(options.account.server_port, 5060U);
    EXPECT_EQ(options.endpoint.listen_port, 0U);
    EXPECT_EQ(options.endpoint.transport, SipTransport::Udp);
    EXPECT_EQ(options.account.transport, SipTransport::Udp);
    EXPECT_EQ(options.registration_timeout, std::chrono::milliseconds{10'000});
    EXPECT_EQ(options.call_timeout, std::chrono::milliseconds{30'000});
    EXPECT_EQ(options.hold_duration, std::chrono::milliseconds{2'000});
    EXPECT_EQ(options.password_environment, "VOIP_SIP_PASSWORD");
}

TEST(SipOutgoingCallCliTest, ParsesTransportPortsRealmTimeoutsAndPasswordEnvironment)
{
    constexpr std::array arguments{std::string_view{"--username"},
                                   std::string_view{"1002"},
                                   std::string_view{"--server"},
                                   std::string_view{"pbx.local"},
                                   std::string_view{"--target"},
                                   std::string_view{"1001"},
                                   std::string_view{"--server-port"},
                                   std::string_view{"5070"},
                                   std::string_view{"--listen-port"},
                                   std::string_view{"5072"},
                                   std::string_view{"--transport"},
                                   std::string_view{"tcp"},
                                   std::string_view{"--realm"},
                                   std::string_view{"asterisk"},
                                   std::string_view{"--registration-timeout-ms"},
                                   std::string_view{"5000"},
                                   std::string_view{"--call-timeout-ms"},
                                   std::string_view{"45000"},
                                   std::string_view{"--hold-ms"},
                                   std::string_view{"1500"},
                                   std::string_view{"--password-env"},
                                   std::string_view{"TEST_SIP_PASSWORD"}};

    const auto options = voip::pjsip::parse_outgoing_call_cli(arguments);

    EXPECT_EQ(options.account.server_port, 5070U);
    EXPECT_EQ(options.endpoint.listen_port, 5072U);
    EXPECT_EQ(options.endpoint.transport, SipTransport::Tcp);
    EXPECT_EQ(options.account.transport, SipTransport::Tcp);
    EXPECT_EQ(options.account.realm, "asterisk");
    EXPECT_EQ(options.registration_timeout, std::chrono::milliseconds{5000});
    EXPECT_EQ(options.call_timeout, std::chrono::milliseconds{45'000});
    EXPECT_EQ(options.hold_duration, std::chrono::milliseconds{1500});
    EXPECT_EQ(options.password_environment, "TEST_SIP_PASSWORD");
}

TEST(SipOutgoingCallCliTest, ParsesAudioDeviceSelectors)
{
    constexpr std::array arguments{
        std::string_view{"--username"},        std::string_view{"1002"},
        std::string_view{"--server"},          std::string_view{"pbx.local"},
        std::string_view{"--target"},          std::string_view{"1001"},
        std::string_view{"--capture-device"},  std::string_view{"USB Headset"},
        std::string_view{"--capture-driver"},  std::string_view{"ALSA"},
        std::string_view{"--playback-device"}, std::string_view{"default"},
        std::string_view{"--playback-driver"}, std::string_view{"ALSA"}};

    const auto options = voip::pjsip::parse_outgoing_call_cli(arguments);

    EXPECT_EQ(options.capture_device.name, "USB Headset");
    EXPECT_EQ(options.capture_device.driver.value_or(""), "ALSA");
    EXPECT_EQ(options.playback_device.name, "default");
    EXPECT_EQ(options.playback_device.driver.value_or(""), "ALSA");
}

TEST(SipOutgoingCallCliTest, BuildsDestinationUri)
{
    constexpr std::array arguments{
        std::string_view{"--username"},  std::string_view{"1002"},
        std::string_view{"--server"},    std::string_view{"192.168.1.13"},
        std::string_view{"--target"},    std::string_view{"1001"},
        std::string_view{"--transport"}, std::string_view{"udp"}};

    auto options = voip::pjsip::parse_outgoing_call_cli(arguments);
    options.account.password = "test-only-password";

    EXPECT_EQ(options.destination_uri(), "sip:1001@192.168.1.13:5060;transport=udp");
}

TEST(SipOutgoingCallCliTest, HelpDoesNotRequireCallArguments)
{
    constexpr std::array arguments{std::string_view{"--help"}};

    const auto options = voip::pjsip::parse_outgoing_call_cli(arguments);

    EXPECT_TRUE(options.show_help);
}

TEST(SipOutgoingCallCliTest, RejectsMissingRequiredArguments)
{
    constexpr std::array missing_username{std::string_view{"--server"},
                                          std::string_view{"192.168.1.13"},
                                          std::string_view{"--target"}, std::string_view{"1001"}};
    constexpr std::array missing_server{std::string_view{"--username"}, std::string_view{"1002"},
                                        std::string_view{"--target"}, std::string_view{"1001"}};
    constexpr std::array missing_target{std::string_view{"--username"}, std::string_view{"1002"},
                                        std::string_view{"--server"},
                                        std::string_view{"192.168.1.13"}};

    EXPECT_THROW(voip::pjsip::parse_outgoing_call_cli(missing_username), std::invalid_argument);
    EXPECT_THROW(voip::pjsip::parse_outgoing_call_cli(missing_server), std::invalid_argument);
    EXPECT_THROW(voip::pjsip::parse_outgoing_call_cli(missing_target), std::invalid_argument);
}

TEST(SipOutgoingCallCliTest, RejectsInvalidTarget)
{
    constexpr std::array arguments{
        std::string_view{"--username"}, std::string_view{"1002"},
        std::string_view{"--server"},   std::string_view{"192.168.1.13"},
        std::string_view{"--target"},   std::string_view{"sip:1001@192.168.1.13"}};

    EXPECT_THROW(voip::pjsip::parse_outgoing_call_cli(arguments), std::invalid_argument);
}

TEST(SipOutgoingCallCliTest, RejectsInvalidNumericValuesAndTransport)
{
    constexpr std::array bad_port{std::string_view{"--username"},    std::string_view{"1002"},
                                  std::string_view{"--server"},      std::string_view{"pbx.local"},
                                  std::string_view{"--target"},      std::string_view{"1001"},
                                  std::string_view{"--server-port"}, std::string_view{"0"}};
    constexpr std::array bad_timeout{
        std::string_view{"--username"},        std::string_view{"1002"},
        std::string_view{"--server"},          std::string_view{"pbx.local"},
        std::string_view{"--target"},          std::string_view{"1001"},
        std::string_view{"--call-timeout-ms"}, std::string_view{"0"}};
    constexpr std::array bad_transport{
        std::string_view{"--username"},  std::string_view{"1002"},     std::string_view{"--server"},
        std::string_view{"pbx.local"},   std::string_view{"--target"}, std::string_view{"1001"},
        std::string_view{"--transport"}, std::string_view{"tls"}};

    EXPECT_THROW(voip::pjsip::parse_outgoing_call_cli(bad_port), std::invalid_argument);
    EXPECT_THROW(voip::pjsip::parse_outgoing_call_cli(bad_timeout), std::invalid_argument);
    EXPECT_THROW(voip::pjsip::parse_outgoing_call_cli(bad_transport), std::invalid_argument);
}

TEST(SipOutgoingCallCliTest, RejectsUnknownOptionAndMissingValue)
{
    constexpr std::array unknown{std::string_view{"--wat"}};
    constexpr std::array missing_value{std::string_view{"--username"}};

    EXPECT_THROW(voip::pjsip::parse_outgoing_call_cli(unknown), std::invalid_argument);
    EXPECT_THROW(voip::pjsip::parse_outgoing_call_cli(missing_value), std::invalid_argument);
}

} // namespace
