#include "network/UdpSocket.hpp"

#include <array>
#include <cstddef>
#include <stdexcept>

#include <gtest/gtest.h>

namespace {

TEST(UdpSocketTest, StartsUnbound)
{
    voip::network::UdpSocket socket{{"127.0.0.1", 0U}};

    EXPECT_FALSE(socket.is_bound());
    EXPECT_EQ(socket.native_handle(), -1);
    EXPECT_THROW(static_cast<void>(socket.local_endpoint()), std::logic_error);
}

TEST(UdpSocketTest, BindsLoopbackWithEphemeralPort)
{
    voip::network::UdpSocket socket{{"127.0.0.1", 0U}};

    socket.bind();

    ASSERT_TRUE(socket.is_bound());
    EXPECT_GE(socket.native_handle(), 0);
    EXPECT_EQ(socket.local_endpoint().address(), "127.0.0.1");
    EXPECT_NE(socket.local_endpoint().port(), 0U);
}

TEST(UdpSocketTest, BindIsIdempotent)
{
    voip::network::UdpSocket socket{{"127.0.0.1", 0U}};

    socket.bind();
    const auto first_handle = socket.native_handle();
    const auto first_endpoint = socket.local_endpoint();

    socket.bind();

    EXPECT_EQ(socket.native_handle(), first_handle);
    EXPECT_EQ(socket.local_endpoint(), first_endpoint);
}

TEST(UdpSocketTest, CloseClosesSocketAndClearsLocalEndpoint)
{
    voip::network::UdpSocket socket{{"127.0.0.1", 0U}};
    socket.bind();

    socket.close();

    EXPECT_FALSE(socket.is_bound());
    EXPECT_EQ(socket.native_handle(), -1);
    EXPECT_THROW(static_cast<void>(socket.local_endpoint()), std::logic_error);
}

TEST(UdpSocketTest, SendRequiresBoundSocket)
{
    voip::network::UdpSocket socket{{"127.0.0.1", 0U}};
    const std::array<std::byte, 1U> payload{std::byte{0x42}};

    EXPECT_THROW(static_cast<void>(socket.send_to(payload, {"127.0.0.1", 9U})), std::logic_error);
}

TEST(UdpSocketTest, ReceiveRequiresBoundSocket)
{
    voip::network::UdpSocket socket{{"127.0.0.1", 0U}};
    std::array<std::byte, 8U> buffer{};

    EXPECT_THROW(static_cast<void>(socket.receive_from(buffer)), std::logic_error);
}

} // namespace
