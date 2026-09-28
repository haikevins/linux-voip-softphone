#include "pjsip/SipEndpointConfig.hpp"

#include <gtest/gtest.h>

namespace {

TEST(SipEndpointConfigTest, MapsUdpTransport)
{
    EXPECT_EQ(voip::pjsip::to_string(voip::pjsip::SipTransport::Udp), "udp");
    EXPECT_EQ(voip::pjsip::to_pjsip_transport_type(voip::pjsip::SipTransport::Udp),
              PJSIP_TRANSPORT_UDP);
}

TEST(SipEndpointConfigTest, MapsTcpTransport)
{
    EXPECT_EQ(voip::pjsip::to_string(voip::pjsip::SipTransport::Tcp), "tcp");
    EXPECT_EQ(voip::pjsip::to_pjsip_transport_type(voip::pjsip::SipTransport::Tcp),
              PJSIP_TRANSPORT_TCP);
}

TEST(SipEndpointConfigTest, MapsListenPortToPjsua2TransportConfig)
{
    const voip::pjsip::SipEndpointConfig endpoint_config{
        .transport = voip::pjsip::SipTransport::Udp,
        .listen_port = 5062U,
    };

    const auto pjsua_config = endpoint_config.to_pjsua2_transport_config();

    EXPECT_EQ(pjsua_config.port, 5062U);
}

} // namespace
