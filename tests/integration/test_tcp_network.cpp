#include "network/Ipv4Endpoint.hpp"
#include "network/TcpClientSocket.hpp"
#include "network/TcpServerSocket.hpp"

#include <cerrno>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

#include <sys/socket.h>

#include <gtest/gtest.h>

namespace {

void send_all(const int socket_fd, const std::string_view payload)
{
    std::size_t offset = 0U;
    while (offset < payload.size()) {
        const auto sent = ::send(socket_fd, payload.data() + offset, payload.size() - offset, 0);
        if (sent == -1) {
            if (errno == EINTR) {
                continue;
            }
            throw std::system_error(errno, std::generic_category(), "send TCP payload");
        }
        if (sent == 0) {
            throw std::runtime_error("TCP send made no progress");
        }
        offset += static_cast<std::size_t>(sent);
    }
}

std::string receive_exact(const int socket_fd, const std::size_t size)
{
    std::string payload(size, '\0');
    std::size_t offset = 0U;

    while (offset < size) {
        const auto received = ::recv(socket_fd, payload.data() + offset, size - offset, 0);
        if (received == -1) {
            if (errno == EINTR) {
                continue;
            }
            throw std::system_error(errno, std::generic_category(), "receive TCP payload");
        }
        if (received == 0) {
            throw std::runtime_error("TCP peer closed before the expected payload arrived");
        }
        offset += static_cast<std::size_t>(received);
    }

    return payload;
}

voip::network::Ipv4Endpoint peer_endpoint(const int socket_fd)
{
    sockaddr_in address{};
    auto address_length = static_cast<socklen_t>(sizeof(address));
    if (::getpeername(socket_fd, reinterpret_cast<sockaddr *>(&address), &address_length) == -1) {
        throw std::system_error(errno, std::generic_category(), "query TCP peer endpoint");
    }
    return voip::network::Ipv4Endpoint::from_sockaddr(address);
}

voip::network::Ipv4Endpoint local_endpoint(const int socket_fd)
{
    sockaddr_in address{};
    auto address_length = static_cast<socklen_t>(sizeof(address));
    if (::getsockname(socket_fd, reinterpret_cast<sockaddr *>(&address), &address_length) == -1) {
        throw std::system_error(errno, std::generic_category(), "query TCP local endpoint");
    }
    return voip::network::Ipv4Endpoint::from_sockaddr(address);
}

TEST(TcpNetworkIntegrationTest, RoundTripsPayload)
{
    voip::network::TcpServerSocket server{voip::network::Ipv4Endpoint{"127.0.0.1", 0U}};
    server.start();

    voip::network::TcpClientSocket client{server.local_endpoint()};
    client.connect();
    auto accepted = server.accept_one();

    constexpr std::string_view request{"phase3-tcp-request"};
    constexpr std::string_view response{"phase3-tcp-response"};

    send_all(client.native_handle(), request);
    EXPECT_EQ(receive_exact(accepted.get(), request.size()), std::string{request});

    send_all(accepted.get(), response);
    EXPECT_EQ(receive_exact(client.native_handle(), response.size()), std::string{response});
}

TEST(TcpNetworkIntegrationTest, ReportsMatchingPeerAndLocalEndpoints)
{
    voip::network::TcpServerSocket server{voip::network::Ipv4Endpoint{"127.0.0.1", 0U}};
    server.start();

    voip::network::TcpClientSocket client{server.local_endpoint()};
    client.connect();
    auto accepted = server.accept_one();

    EXPECT_EQ(client.remote_endpoint(), server.local_endpoint());
    EXPECT_EQ(local_endpoint(accepted.get()), server.local_endpoint());
    EXPECT_EQ(peer_endpoint(accepted.get()), client.local_endpoint());
    EXPECT_EQ(client.local_endpoint().address(), "127.0.0.1");
    EXPECT_NE(client.local_endpoint().port(), 0U);
}

TEST(TcpNetworkIntegrationTest, AcceptsSequentialClientConnections)
{
    voip::network::TcpServerSocket server{voip::network::Ipv4Endpoint{"127.0.0.1", 0U}};
    server.start();

    constexpr std::string_view first_payload{"first-client"};
    {
        voip::network::TcpClientSocket first_client{server.local_endpoint()};
        first_client.connect();
        auto accepted = server.accept_one();

        send_all(first_client.native_handle(), first_payload);
        EXPECT_EQ(receive_exact(accepted.get(), first_payload.size()), std::string{first_payload});
    }

    constexpr std::string_view second_payload{"second-client"};
    {
        voip::network::TcpClientSocket second_client{server.local_endpoint()};
        second_client.connect();
        auto accepted = server.accept_one();

        send_all(second_client.native_handle(), second_payload);
        EXPECT_EQ(receive_exact(accepted.get(), second_payload.size()),
                  std::string{second_payload});
    }
}

} // namespace
