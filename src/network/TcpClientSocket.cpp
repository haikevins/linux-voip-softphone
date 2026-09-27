#include "network/TcpClientSocket.hpp"

#include <cerrno>
#include <stdexcept>
#include <system_error>
#include <utility>

#include <sys/socket.h>

namespace voip::network {

TcpClientSocket::TcpClientSocket(Ipv4Endpoint remote_endpoint) : remote_endpoint_(remote_endpoint)
{}

void TcpClientSocket::connect()
{
    if (socket_) {
        return;
    }

    platform::UniqueFd socket{::socket(AF_INET, SOCK_STREAM, 0)};
    if (!socket) {
        throw std::system_error(errno, std::generic_category(), "create TCP client socket");
    }

    const auto remote_address = remote_endpoint_.to_sockaddr();
    int connect_result = -1;
    do {
        connect_result =
            ::connect(socket.get(), reinterpret_cast<const sockaddr *>(&remote_address),
                      static_cast<socklen_t>(sizeof(remote_address)));
    } while (connect_result == -1 && errno == EINTR);

    if (connect_result == -1) {
        throw std::system_error(errno, std::generic_category(), "connect TCP socket");
    }

    sockaddr_in local_address{};
    auto local_address_length = static_cast<socklen_t>(sizeof(local_address));
    if (::getsockname(socket.get(), reinterpret_cast<sockaddr *>(&local_address),
                      &local_address_length) == -1) {
        throw std::system_error(errno, std::generic_category(), "query TCP client local endpoint");
    }

    local_endpoint_ = Ipv4Endpoint::from_sockaddr(local_address);
    socket_ = std::move(socket);
}

void TcpClientSocket::disconnect() noexcept
{
    socket_.reset();
    local_endpoint_.reset();
}

bool TcpClientSocket::is_connected() const noexcept
{
    return static_cast<bool>(socket_);
}

int TcpClientSocket::native_handle() const noexcept
{
    return socket_.get();
}

const Ipv4Endpoint &TcpClientSocket::remote_endpoint() const noexcept
{
    return remote_endpoint_;
}

Ipv4Endpoint TcpClientSocket::local_endpoint() const
{
    if (!local_endpoint_.has_value()) {
        throw std::logic_error("TCP client socket is not connected");
    }

    return *local_endpoint_;
}

} // namespace voip::network
