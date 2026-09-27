#include "network/TcpServerSocket.hpp"

#include <cstdint>
#include <stdexcept>

#include <gtest/gtest.h>

namespace {

TEST(TcpServerSocketTest, StartsStopped)
{
    voip::network::TcpServerSocket server{voip::network::Ipv4Endpoint{"127.0.0.1", 0U}};

    EXPECT_FALSE(server.is_listening());
    EXPECT_THROW(static_cast<void>(server.local_endpoint()), std::logic_error);
}

TEST(TcpServerSocketTest, BindsLoopbackWithEphemeralPort)
{
    voip::network::TcpServerSocket server{voip::network::Ipv4Endpoint{"127.0.0.1", 0U}};

    server.start();

    ASSERT_TRUE(server.is_listening());
    const auto endpoint = server.local_endpoint();
    EXPECT_EQ(endpoint.address(), "127.0.0.1");
    EXPECT_NE(endpoint.port(), 0U);
}

TEST(TcpServerSocketTest, StartIsIdempotent)
{
    voip::network::TcpServerSocket server{voip::network::Ipv4Endpoint{"127.0.0.1", 0U}};

    server.start();
    const auto first_endpoint = server.local_endpoint();

    EXPECT_NO_THROW(server.start());
    EXPECT_EQ(server.local_endpoint(), first_endpoint);
}

TEST(TcpServerSocketTest, StopClosesListenerAndClearsLocalEndpoint)
{
    voip::network::TcpServerSocket server{voip::network::Ipv4Endpoint{"127.0.0.1", 0U}};
    server.start();

    server.stop();

    EXPECT_FALSE(server.is_listening());
    EXPECT_THROW(static_cast<void>(server.local_endpoint()), std::logic_error);
    EXPECT_NO_THROW(server.stop());
}

TEST(TcpServerSocketTest, AcceptRequiresListeningSocket)
{
    voip::network::TcpServerSocket server{voip::network::Ipv4Endpoint{"127.0.0.1", 0U}};

    EXPECT_THROW(static_cast<void>(server.accept_one()), std::logic_error);
}

} // namespace
