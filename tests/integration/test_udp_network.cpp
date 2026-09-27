#include "network/Ipv4Endpoint.hpp"
#include "network/UdpSocket.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>

#include <gtest/gtest.h>

namespace {

using ByteBuffer = std::array<std::byte, 64U>;

TEST(UdpNetworkIntegrationTest, RoundTripsDatagramsBidirectionally)
{
    voip::network::UdpSocket first{voip::network::Ipv4Endpoint{"127.0.0.1", 0U}};
    voip::network::UdpSocket second{voip::network::Ipv4Endpoint{"127.0.0.1", 0U}};
    first.bind();
    second.bind();

    constexpr std::array request{std::byte{0x50}, std::byte{0x33}, std::byte{0x2D},
                                 std::byte{0x55}};
    EXPECT_EQ(first.send_to(request, second.local_endpoint()), request.size());

    ByteBuffer request_buffer{};
    const auto request_result = second.receive_from(request_buffer);
    EXPECT_EQ(request_result.bytes_received, request.size());
    EXPECT_EQ(request_result.peer_endpoint, first.local_endpoint());
    EXPECT_TRUE(std::equal(request.begin(), request.end(), request_buffer.begin()));

    constexpr std::array response{std::byte{0x4F}, std::byte{0x4B}};
    EXPECT_EQ(second.send_to(response, request_result.peer_endpoint), response.size());

    ByteBuffer response_buffer{};
    const auto response_result = first.receive_from(response_buffer);
    EXPECT_EQ(response_result.bytes_received, response.size());
    EXPECT_EQ(response_result.peer_endpoint, second.local_endpoint());
    EXPECT_TRUE(std::equal(response.begin(), response.end(), response_buffer.begin()));
}

TEST(UdpNetworkIntegrationTest, PreservesBinaryPayloadAndReportsSenderEndpoint)
{
    voip::network::UdpSocket sender{voip::network::Ipv4Endpoint{"127.0.0.1", 0U}};
    voip::network::UdpSocket receiver{voip::network::Ipv4Endpoint{"127.0.0.1", 0U}};
    sender.bind();
    receiver.bind();

    constexpr std::array payload{std::byte{0x00}, std::byte{0x01}, std::byte{0x7F},
                                 std::byte{0x80}, std::byte{0xFE}, std::byte{0xFF}};
    EXPECT_EQ(sender.send_to(payload, receiver.local_endpoint()), payload.size());

    ByteBuffer buffer{};
    const auto result = receiver.receive_from(buffer);

    EXPECT_EQ(result.bytes_received, payload.size());
    EXPECT_EQ(result.peer_endpoint, sender.local_endpoint());
    EXPECT_TRUE(std::equal(payload.begin(), payload.end(), buffer.begin()));
}

TEST(UdpNetworkIntegrationTest, ReceivesSequentialDatagrams)
{
    voip::network::UdpSocket sender{voip::network::Ipv4Endpoint{"127.0.0.1", 0U}};
    voip::network::UdpSocket receiver{voip::network::Ipv4Endpoint{"127.0.0.1", 0U}};
    sender.bind();
    receiver.bind();

    constexpr std::array first_payload{std::byte{0x01}, std::byte{0x02}, std::byte{0x03}};
    constexpr std::array second_payload{std::byte{0xA0}, std::byte{0xB0}, std::byte{0xC0},
                                        std::byte{0xD0}};

    EXPECT_EQ(sender.send_to(first_payload, receiver.local_endpoint()), first_payload.size());
    EXPECT_EQ(sender.send_to(second_payload, receiver.local_endpoint()), second_payload.size());

    ByteBuffer first_buffer{};
    const auto first_result = receiver.receive_from(first_buffer);
    EXPECT_EQ(first_result.bytes_received, first_payload.size());
    EXPECT_EQ(first_result.peer_endpoint, sender.local_endpoint());
    EXPECT_TRUE(std::equal(first_payload.begin(), first_payload.end(), first_buffer.begin()));

    ByteBuffer second_buffer{};
    const auto second_result = receiver.receive_from(second_buffer);
    EXPECT_EQ(second_result.bytes_received, second_payload.size());
    EXPECT_EQ(second_result.peer_endpoint, sender.local_endpoint());
    EXPECT_TRUE(std::equal(second_payload.begin(), second_payload.end(), second_buffer.begin()));
}

} // namespace
