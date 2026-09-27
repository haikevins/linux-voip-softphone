#include "ipc/UnixDomainSocketClient.hpp"
#include "ipc/UnixDomainSocketServer.hpp"

#include <cerrno>
#include <stdexcept>
#include <string>
#include <system_error>

#include <gtest/gtest.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace {

std::string unique_socket_path()
{
    static unsigned int sequence = 0U;
    ++sequence;
    return "/tmp/linux-voip-softphone-uds-client-" + std::to_string(::getpid()) + "-" +
           std::to_string(sequence) + ".sock";
}

TEST(UnixDomainSocketClientTest, StartsDisconnected)
{
    const auto socket_path = unique_socket_path();
    voip::ipc::UnixDomainSocketClient client{socket_path};

    EXPECT_FALSE(client.is_connected());
    EXPECT_EQ(client.native_handle(), -1);
    EXPECT_EQ(client.path(), socket_path);
}

TEST(UnixDomainSocketClientTest, ConnectsToListeningServer)
{
    const auto socket_path = unique_socket_path();
    voip::ipc::UnixDomainSocketServer server{socket_path};
    server.start();

    voip::ipc::UnixDomainSocketClient client{socket_path};
    client.connect();
    auto peer = server.accept_one();

    EXPECT_TRUE(client.is_connected());
    EXPECT_GE(client.native_handle(), 0);
    EXPECT_TRUE(static_cast<bool>(peer));
}

TEST(UnixDomainSocketClientTest, DisconnectClosesConnection)
{
    const auto socket_path = unique_socket_path();
    voip::ipc::UnixDomainSocketServer server{socket_path};
    server.start();

    voip::ipc::UnixDomainSocketClient client{socket_path};
    client.connect();
    auto peer = server.accept_one();

    client.disconnect();

    char byte = '\0';
    EXPECT_EQ(::recv(peer.get(), &byte, sizeof(byte), 0), 0);
    EXPECT_FALSE(client.is_connected());
}

TEST(UnixDomainSocketClientTest, RejectsOverlongSocketPath)
{
    std::string socket_path(sizeof(sockaddr_un::sun_path), 'x');
    voip::ipc::UnixDomainSocketClient client{socket_path};

    EXPECT_THROW(client.connect(), std::invalid_argument);
    EXPECT_FALSE(client.is_connected());
}

TEST(UnixDomainSocketClientTest, ReportsMissingServer)
{
    const auto socket_path = unique_socket_path();
    static_cast<void>(::unlink(socket_path.c_str()));
    voip::ipc::UnixDomainSocketClient client{socket_path};

    try {
        client.connect();
        FAIL() << "expected connect() to fail when no server is listening";
    } catch (const std::system_error &error) {
        EXPECT_EQ(error.code().value(), ENOENT);
    }

    EXPECT_FALSE(client.is_connected());
}

} // namespace
