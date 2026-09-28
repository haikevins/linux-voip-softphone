#include "pjsip/SipAccountConfig.hpp"

#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

namespace {

[[nodiscard]] voip::pjsip::SipAccountConfig make_account_config()
{
    return voip::pjsip::SipAccountConfig{
        .username = "1002",
        .password = "voip-lab-1002",
        .server = "192.168.1.23",
        .server_port = 5060U,
        .realm = "*",
        .transport = voip::pjsip::SipTransport::Udp,
        .register_on_add = true,
    };
}

TEST(SipAccountConfigTest, BuildsIdentityAndRegistrarUris)
{
    const auto config = make_account_config();

    EXPECT_EQ(config.identity_uri(), "sip:1002@192.168.1.23");
    EXPECT_EQ(config.registrar_uri(), "sip:192.168.1.23:5060;transport=udp");
}

TEST(SipAccountConfigTest, SupportsTcpRegistrarTransport)
{
    auto config = make_account_config();
    config.transport = voip::pjsip::SipTransport::Tcp;

    EXPECT_EQ(config.registrar_uri(), "sip:192.168.1.23:5060;transport=tcp");
}

TEST(SipAccountConfigTest, MapsToPjsua2AccountConfig)
{
    const auto config = make_account_config();

    const auto pjsua_config = config.to_pjsua2_account_config();

    EXPECT_EQ(pjsua_config.idUri, "sip:1002@192.168.1.23");
    EXPECT_EQ(pjsua_config.regConfig.registrarUri, "sip:192.168.1.23:5060;transport=udp");
    EXPECT_TRUE(pjsua_config.regConfig.registerOnAdd);
    ASSERT_EQ(pjsua_config.sipConfig.authCreds.size(), 1U);

    const auto &credential = pjsua_config.sipConfig.authCreds.front();
    EXPECT_EQ(credential.scheme, "digest");
    EXPECT_EQ(credential.realm, "*");
    EXPECT_EQ(credential.username, "1002");
    EXPECT_EQ(credential.dataType, PJSIP_CRED_DATA_PLAIN_PASSWD);
    EXPECT_EQ(credential.data, "voip-lab-1002");
}

TEST(SipAccountConfigTest, PreservesManualRegistrationChoice)
{
    auto config = make_account_config();
    config.register_on_add = false;

    const auto pjsua_config = config.to_pjsua2_account_config();

    EXPECT_FALSE(pjsua_config.regConfig.registerOnAdd);
}

TEST(SipAccountConfigTest, RejectsMissingRequiredValues)
{
    auto config = make_account_config();
    config.username.clear();
    EXPECT_THROW(config.validate(), std::invalid_argument);

    config = make_account_config();
    config.password.clear();
    EXPECT_THROW(config.validate(), std::invalid_argument);

    config = make_account_config();
    config.server.clear();
    EXPECT_THROW(config.validate(), std::invalid_argument);

    config = make_account_config();
    config.realm.clear();
    EXPECT_THROW(config.validate(), std::invalid_argument);
}

TEST(SipAccountConfigTest, RejectsZeroServerPort)
{
    auto config = make_account_config();
    config.server_port = 0U;

    EXPECT_THROW(config.validate(), std::invalid_argument);
}

TEST(SipAccountConfigTest, RejectsPreformattedServerUri)
{
    auto config = make_account_config();
    config.server = "sip:192.168.1.23:5060";

    EXPECT_THROW(config.validate(), std::invalid_argument);
}

TEST(SipAccountConfigTest, RejectsWhitespaceInUriComponents)
{
    auto config = make_account_config();
    config.username = "10 02";
    EXPECT_THROW(config.validate(), std::invalid_argument);

    config = make_account_config();
    config.server = "asterisk local";
    EXPECT_THROW(config.validate(), std::invalid_argument);

    config = make_account_config();
    config.realm = "lab realm";
    EXPECT_THROW(config.validate(), std::invalid_argument);
}

} // namespace
