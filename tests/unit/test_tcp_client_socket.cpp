#include "network/TcpClientSocket.hpp"
#include "network/TcpServerSocket.hpp"

#include <stdexcept>

#include <gtest/gtest.h>

namespace {

TEST(TcpClientSocketTest, StartsDisconnected)
{
    const voip::network::Ipv4Endpoint remote{"127.0.0.1", 5060U};
    voip::network::TcpClientSocket client{remote};

    EXPECT_FALSE(client.is_connected());
    EXPECT_EQ(client.native_handle(), voip::platform::UniqueFd::invalid_fd);
    EXPECT_EQ(client.remote_endpoint(), remote);
    EXPECT_THROW(static_cast<void>(client.local_endpoint()), std::logic_error);
}

TEST(TcpClientSocketTest, ConnectsToListeningServer)
{
    voip::network::TcpServerSocket server{voip::network::Ipv4Endpoint{"127.0.0.1", 0U}};
    server.start();

    voip::network::TcpClientSocket client{server.local_endpoint()};
    client.connect();

    ASSERT_TRUE(client.is_connected());
    EXPECT_NE(client.native_handle(), voip::platform::UniqueFd::invalid_fd);
    EXPECT_EQ(client.remote_endpoint(), server.local_endpoint());

    const auto local = client.local_endpoint();
    EXPECT_EQ(local.address(), "127.0.0.1");
    EXPECT_NE(local.port(), 0U);
}

TEST(TcpClientSocketTest, ConnectIsIdempotent)
{
    voip::network::TcpServerSocket server{voip::network::Ipv4Endpoint{"127.0.0.1", 0U}};
    server.start();

    voip::network::TcpClientSocket client{server.local_endpoint()};
    client.connect();
    const auto first_handle = client.native_handle();
    const auto first_local = client.local_endpoint();

    EXPECT_NO_THROW(client.connect());
    EXPECT_EQ(client.native_handle(), first_handle);
    EXPECT_EQ(client.local_endpoint(), first_local);
}

TEST(TcpClientSocketTest, DisconnectClosesSocketAndClearsLocalEndpoint)
{
    voip::network::TcpServerSocket server{voip::network::Ipv4Endpoint{"127.0.0.1", 0U}};
    server.start();

    voip::network::TcpClientSocket client{server.local_endpoint()};
    client.connect();

    client.disconnect();

    EXPECT_FALSE(client.is_connected());
    EXPECT_EQ(client.native_handle(), voip::platform::UniqueFd::invalid_fd);
    EXPECT_THROW(static_cast<void>(client.local_endpoint()), std::logic_error);
    EXPECT_NO_THROW(client.disconnect());
}

TEST(TcpClientSocketTest, CanReconnectAfterDisconnect)
{
    voip::network::TcpServerSocket server{voip::network::Ipv4Endpoint{"127.0.0.1", 0U}};
    server.start();

    voip::network::TcpClientSocket client{server.local_endpoint()};
    client.connect();
    client.disconnect();

    EXPECT_NO_THROW(client.connect());
    EXPECT_TRUE(client.is_connected());
    EXPECT_NE(client.local_endpoint().port(), 0U);
}

} // namespace
