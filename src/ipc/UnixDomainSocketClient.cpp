#include "ipc/UnixDomainSocketClient.hpp"

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <system_error>
#include <utility>

#include <sys/socket.h>
#include <sys/un.h>

namespace voip::ipc {
namespace {

sockaddr_un make_address(const std::string &path)
{
    sockaddr_un address{};
    address.sun_family = AF_UNIX;

    if (path.empty()) {
        throw std::invalid_argument("Unix socket path must not be empty");
    }

    if (path.size() >= sizeof(address.sun_path)) {
        throw std::invalid_argument("Unix socket path is too long");
    }

    std::memcpy(address.sun_path, path.c_str(), path.size() + 1U);
    return address;
}

} // namespace

UnixDomainSocketClient::UnixDomainSocketClient(std::string socket_path)
    : socket_path_(std::move(socket_path))
{}

void UnixDomainSocketClient::connect()
{
    if (socket_) {
        return;
    }

    const auto address = make_address(socket_path_);
    platform::UniqueFd socket{::socket(AF_UNIX, SOCK_STREAM, 0)};
    if (!socket) {
        throw std::system_error(errno, std::generic_category(), "create Unix domain socket");
    }

    int connect_result = -1;
    do {
        connect_result = ::connect(socket.get(), reinterpret_cast<const sockaddr *>(&address),
                                   static_cast<socklen_t>(sizeof(address)));
    } while (connect_result == -1 && errno == EINTR);

    if (connect_result == -1) {
        throw std::system_error(errno, std::generic_category(), "connect Unix domain socket");
    }

    socket_ = std::move(socket);
}

void UnixDomainSocketClient::disconnect() noexcept
{
    socket_.reset();
}

bool UnixDomainSocketClient::is_connected() const noexcept
{
    return static_cast<bool>(socket_);
}

int UnixDomainSocketClient::native_handle() const noexcept
{
    return socket_.get();
}

const std::string &UnixDomainSocketClient::path() const noexcept
{
    return socket_path_;
}

} // namespace voip::ipc
