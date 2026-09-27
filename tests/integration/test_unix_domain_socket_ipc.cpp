#include "ipc/UnixDomainSocketClient.hpp"
#include "ipc/UnixDomainSocketServer.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <string>
#include <string_view>

#include <gtest/gtest.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

std::string unique_socket_path()
{
    static unsigned int sequence = 0U;
    ++sequence;
    return "/tmp/linux-voip-softphone-ipc-integration-" + std::to_string(::getpid()) + "-" +
           std::to_string(sequence) + ".sock";
}

bool send_all(const int fd, const std::string_view payload)
{
    std::size_t sent = 0U;
    while (sent < payload.size()) {
        const auto result = ::send(fd, payload.data() + sent, payload.size() - sent, MSG_NOSIGNAL);
        if (result > 0) {
            sent += static_cast<std::size_t>(result);
            continue;
        }
        if (result == -1 && errno == EINTR) {
            continue;
        }
        return false;
    }
    return true;
}

bool receive_exact(const int fd, char *const destination, const std::size_t size)
{
    std::size_t received = 0U;
    while (received < size) {
        const auto result = ::recv(fd, destination + received, size - received, 0);
        if (result > 0) {
            received += static_cast<std::size_t>(result);
            continue;
        }
        if (result == -1 && errno == EINTR) {
            continue;
        }
        return false;
    }
    return true;
}

TEST(UnixDomainSocketIpcIntegrationTest, RoundTripsRequestAndResponse)
{
    const auto socket_path = unique_socket_path();
    voip::ipc::UnixDomainSocketServer server{socket_path};
    server.start();

    voip::ipc::UnixDomainSocketClient client{socket_path};
    client.connect();
    auto server_peer = server.accept_one();

    constexpr std::string_view request{"PING"};
    constexpr std::string_view response{"PONG"};
    std::array<char, request.size()> request_buffer{};
    std::array<char, response.size()> response_buffer{};

    ASSERT_TRUE(send_all(client.native_handle(), request));
    ASSERT_TRUE(receive_exact(server_peer.get(), request_buffer.data(), request_buffer.size()));
    EXPECT_EQ(std::string_view(request_buffer.data(), request_buffer.size()), request);

    ASSERT_TRUE(send_all(server_peer.get(), response));
    ASSERT_TRUE(
        receive_exact(client.native_handle(), response_buffer.data(), response_buffer.size()));
    EXPECT_EQ(std::string_view(response_buffer.data(), response_buffer.size()), response);
}

TEST(UnixDomainSocketIpcIntegrationTest, AcceptsSequentialClientConnections)
{
    const auto socket_path = unique_socket_path();
    voip::ipc::UnixDomainSocketServer server{socket_path};
    server.start();

    {
        voip::ipc::UnixDomainSocketClient first_client{socket_path};
        first_client.connect();
        auto first_peer = server.accept_one();
        EXPECT_TRUE(first_client.is_connected());
        EXPECT_TRUE(static_cast<bool>(first_peer));
    }

    {
        voip::ipc::UnixDomainSocketClient second_client{socket_path};
        second_client.connect();
        auto second_peer = server.accept_one();
        EXPECT_TRUE(second_client.is_connected());
        EXPECT_TRUE(static_cast<bool>(second_peer));
    }
}

} // namespace
