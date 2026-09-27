#include "network/Ipv4Endpoint.hpp"

#include <cstdint>
#include <stdexcept>

#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <sys/socket.h>

namespace {

TEST(Ipv4EndpointTest, StoresAddressAndPort)
{
    const voip::network::Ipv4Endpoint endpoint{"127.0.0.1", 5060U};

    EXPECT_EQ(endpoint.address(), "127.0.0.1");
    EXPECT_EQ(endpoint.port(), 5060U);
}

TEST(Ipv4EndpointTest, ConvertsToSockaddrUsingNetworkByteOrder)
{
    const voip::network::Ipv4Endpoint endpoint{"192.0.2.10", 40000U};

    const auto address = endpoint.to_sockaddr();

    EXPECT_EQ(address.sin_family, AF_INET);
    EXPECT_EQ(ntohs(address.sin_port), 40000U);

    const auto round_trip = voip::network::Ipv4Endpoint::from_sockaddr(address);
    EXPECT_EQ(round_trip, endpoint);
}

TEST(Ipv4EndpointTest, SupportsWildcardAddress)
{
    const voip::network::Ipv4Endpoint endpoint{"0.0.0.0", 0U};

    EXPECT_EQ(endpoint.address(), "0.0.0.0");
    EXPECT_EQ(endpoint.port(), 0U);
}

TEST(Ipv4EndpointTest, RejectsEmptyAddress)
{
    EXPECT_THROW((voip::network::Ipv4Endpoint{"", 5060U}), std::invalid_argument);
}

TEST(Ipv4EndpointTest, RejectsMalformedIpv4Address)
{
    EXPECT_THROW((voip::network::Ipv4Endpoint{"999.1.2.3", 5060U}), std::invalid_argument);
    EXPECT_THROW((voip::network::Ipv4Endpoint{"127.0.0.1.example", 5060U}), std::invalid_argument);
}

TEST(Ipv4EndpointTest, RejectsNonIpv4Sockaddr)
{
    sockaddr_in address{};
    address.sin_family = AF_UNSPEC;

    EXPECT_THROW(static_cast<void>(voip::network::Ipv4Endpoint::from_sockaddr(address)),
                 std::invalid_argument);
}

} // namespace
