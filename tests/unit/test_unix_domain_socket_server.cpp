#include "ipc/UnixDomainSocketServer.hpp"

#include <cerrno>
#include <stdexcept>
#include <string>

#include <fcntl.h>
#include <gtest/gtest.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

namespace {

std::string unique_socket_path()
{
    static unsigned int sequence = 0U;
    ++sequence;
    return "/tmp/linux-voip-softphone-uds-" + std::to_string(::getpid()) + "-" +
           std::to_string(sequence) + ".sock";
}

bool is_unix_socket(const std::string &path)
{
    struct stat metadata
    {
    };

    return ::lstat(path.c_str(), &metadata) == 0 && S_ISSOCK(metadata.st_mode);
}

TEST(UnixDomainSocketServerTest, StartsAndCreatesSocketPath)
{
    const auto socket_path = unique_socket_path();
    voip::ipc::UnixDomainSocketServer server{socket_path};

    server.start();

    EXPECT_TRUE(server.is_listening());
    EXPECT_EQ(server.path(), socket_path);
    EXPECT_TRUE(is_unix_socket(socket_path));
}

TEST(UnixDomainSocketServerTest, StopRemovesSocketPath)
{
    const auto socket_path = unique_socket_path();
    voip::ipc::UnixDomainSocketServer server{socket_path};
    server.start();

    server.stop();

    EXPECT_FALSE(server.is_listening());
    const int access_result = ::access(socket_path.c_str(), F_OK);
    const int access_error = errno;
    EXPECT_EQ(access_result, -1);
    EXPECT_EQ(access_error, ENOENT);
}

TEST(UnixDomainSocketServerTest, DestructorRemovesSocketPath)
{
    const auto socket_path = unique_socket_path();

    {
        voip::ipc::UnixDomainSocketServer server{socket_path};
        server.start();
        ASSERT_TRUE(is_unix_socket(socket_path));
    }

    const int access_result = ::access(socket_path.c_str(), F_OK);
    const int access_error = errno;
    EXPECT_EQ(access_result, -1);
    EXPECT_EQ(access_error, ENOENT);
}

TEST(UnixDomainSocketServerTest, RejectsOverlongSocketPath)
{
    std::string socket_path(sizeof(sockaddr_un::sun_path), 'x');
    voip::ipc::UnixDomainSocketServer server{socket_path};

    EXPECT_THROW(server.start(), std::invalid_argument);
    EXPECT_FALSE(server.is_listening());
}

TEST(UnixDomainSocketServerTest, RefusesToReplaceRegularFile)
{
    const auto socket_path = unique_socket_path();
    ASSERT_EQ(::close(::creat(socket_path.c_str(), 0600)), 0);

    voip::ipc::UnixDomainSocketServer server{socket_path};
    EXPECT_THROW(server.start(), std::runtime_error);

    EXPECT_EQ(::unlink(socket_path.c_str()), 0);
}

} // namespace
