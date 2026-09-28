#include "pjsip/SipRegistrationCli.hpp"

#include <array>
#include <chrono>
#include <stdexcept>
#include <string_view>

#include <gtest/gtest.h>

namespace {

using voip::pjsip::SipTransport;

TEST(SipRegistrationCliTest, ParsesRequiredValuesAndDefaults)
{
    constexpr std::array arguments{
        std::string_view{"--username"},
        std::string_view{"1002"},
        std::string_view{"--server"},
        std::string_view{"192.168.1.13"},
    };

    const auto options = voip::pjsip::parse_registration_cli(arguments);

    EXPECT_EQ(options.account.username, "1002");
    EXPECT_EQ(options.account.server, "192.168.1.13");
    EXPECT_EQ(options.account.server_port, 5060U);
    EXPECT_EQ(options.endpoint.listen_port, 0U);
    EXPECT_EQ(options.endpoint.transport, SipTransport::Udp);
    EXPECT_EQ(options.account.transport, SipTransport::Udp);
    EXPECT_EQ(options.account.realm, "*");
    EXPECT_EQ(options.timeout, std::chrono::milliseconds{10'000});
    EXPECT_EQ(options.password_environment, "VOIP_SIP_PASSWORD");
}

TEST(SipRegistrationCliTest, ParsesTransportPortsRealmTimeoutAndPasswordEnvironment)
{
    constexpr std::array arguments{
        std::string_view{"--username"},     std::string_view{"1002"},
        std::string_view{"--server"},       std::string_view{"pbx.example.test"},
        std::string_view{"--server-port"},  std::string_view{"5070"},
        std::string_view{"--listen-port"},  std::string_view{"5090"},
        std::string_view{"--transport"},    std::string_view{"tcp"},
        std::string_view{"--realm"},        std::string_view{"asterisk"},
        std::string_view{"--timeout-ms"},   std::string_view{"2500"},
        std::string_view{"--password-env"}, std::string_view{"LAB_SIP_PASSWORD"},
    };

    const auto options = voip::pjsip::parse_registration_cli(arguments);

    EXPECT_EQ(options.account.server_port, 5070U);
    EXPECT_EQ(options.endpoint.listen_port, 5090U);
    EXPECT_EQ(options.endpoint.transport, SipTransport::Tcp);
    EXPECT_EQ(options.account.transport, SipTransport::Tcp);
    EXPECT_EQ(options.account.realm, "asterisk");
    EXPECT_EQ(options.timeout, std::chrono::milliseconds{2500});
    EXPECT_EQ(options.password_environment, "LAB_SIP_PASSWORD");
}

TEST(SipRegistrationCliTest, HelpDoesNotRequireAccountArguments)
{
    constexpr std::array arguments{std::string_view{"--help"}};

    const auto options = voip::pjsip::parse_registration_cli(arguments);

    EXPECT_TRUE(options.show_help);
}

TEST(SipRegistrationCliTest, RejectsMissingRequiredArguments)
{
    constexpr std::array missing_username{
        std::string_view{"--server"},
        std::string_view{"192.168.1.13"},
    };
    constexpr std::array missing_server{
        std::string_view{"--username"},
        std::string_view{"1002"},
    };

    EXPECT_THROW(static_cast<void>(voip::pjsip::parse_registration_cli(missing_username)),
                 std::invalid_argument);
    EXPECT_THROW(static_cast<void>(voip::pjsip::parse_registration_cli(missing_server)),
                 std::invalid_argument);
}

TEST(SipRegistrationCliTest, RejectsInvalidNumericValuesAndTransport)
{
    constexpr std::array invalid_port{
        std::string_view{"--username"},    std::string_view{"1002"},
        std::string_view{"--server"},      std::string_view{"192.168.1.13"},
        std::string_view{"--server-port"}, std::string_view{"70000"},
    };
    constexpr std::array invalid_transport{
        std::string_view{"--username"},  std::string_view{"1002"},
        std::string_view{"--server"},    std::string_view{"192.168.1.13"},
        std::string_view{"--transport"}, std::string_view{"tls"},
    };

    EXPECT_THROW(static_cast<void>(voip::pjsip::parse_registration_cli(invalid_port)),
                 std::invalid_argument);
    EXPECT_THROW(static_cast<void>(voip::pjsip::parse_registration_cli(invalid_transport)),
                 std::invalid_argument);
}

TEST(SipRegistrationCliTest, RejectsUnknownOptionAndMissingValue)
{
    constexpr std::array unknown_option{
        std::string_view{"--username"},   std::string_view{"1002"},    std::string_view{"--server"},
        std::string_view{"192.168.1.13"}, std::string_view{"--bogus"},
    };
    constexpr std::array missing_value{
        std::string_view{"--username"},
        std::string_view{"1002"},
        std::string_view{"--server"},
    };

    EXPECT_THROW(static_cast<void>(voip::pjsip::parse_registration_cli(unknown_option)),
                 std::invalid_argument);
    EXPECT_THROW(static_cast<void>(voip::pjsip::parse_registration_cli(missing_value)),
                 std::invalid_argument);
}

} // namespace
