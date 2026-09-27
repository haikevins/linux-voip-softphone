#include "network/TcpServerSocket.hpp"

#include <cerrno>
#include <stdexcept>
#include <system_error>
#include <utility>

#include <sys/socket.h>

namespace voip::network {

TcpServerSocket::TcpServerSocket(Ipv4Endpoint endpoint) : requested_endpoint_(endpoint) {}

void TcpServerSocket::start()
{
    if (listener_) {
        return;
    }

    platform::UniqueFd listener{::socket(AF_INET, SOCK_STREAM, 0)};
    if (!listener) {
        throw std::system_error(errno, std::generic_category(), "create TCP socket");
    }

    const int reuse_address = 1;
    if (::setsockopt(listener.get(), SOL_SOCKET, SO_REUSEADDR, &reuse_address,
                     static_cast<socklen_t>(sizeof(reuse_address))) == -1) {
        throw std::system_error(errno, std::generic_category(), "set TCP SO_REUSEADDR");
    }

    const auto requested_address = requested_endpoint_.to_sockaddr();
    if (::bind(listener.get(), reinterpret_cast<const sockaddr *>(&requested_address),
               static_cast<socklen_t>(sizeof(requested_address))) == -1) {
        throw std::system_error(errno, std::generic_category(), "bind TCP socket");
    }

    if (::listen(listener.get(), SOMAXCONN) == -1) {
        throw std::system_error(errno, std::generic_category(), "listen on TCP socket");
    }

    sockaddr_in bound_address{};
    auto bound_address_length = static_cast<socklen_t>(sizeof(bound_address));
    if (::getsockname(listener.get(), reinterpret_cast<sockaddr *>(&bound_address),
                      &bound_address_length) == -1) {
        throw std::system_error(errno, std::generic_category(), "query TCP local endpoint");
    }

    local_endpoint_ = Ipv4Endpoint::from_sockaddr(bound_address);
    listener_ = std::move(listener);
}

void TcpServerSocket::stop() noexcept
{
    listener_.reset();
    local_endpoint_.reset();
}

platform::UniqueFd TcpServerSocket::accept_one() const
{
    if (!listener_) {
        throw std::logic_error("TCP server socket is not listening");
    }

    int accepted_fd = platform::UniqueFd::invalid_fd;
    do {
        accepted_fd = ::accept(listener_.get(), nullptr, nullptr);
    } while (accepted_fd == platform::UniqueFd::invalid_fd && errno == EINTR);

    if (accepted_fd == platform::UniqueFd::invalid_fd) {
        throw std::system_error(errno, std::generic_category(), "accept TCP client");
    }

    return platform::UniqueFd{accepted_fd};
}

bool TcpServerSocket::is_listening() const noexcept
{
    return static_cast<bool>(listener_);
}

Ipv4Endpoint TcpServerSocket::local_endpoint() const
{
    if (!local_endpoint_.has_value()) {
        throw std::logic_error("TCP server socket is not listening");
    }

    return *local_endpoint_;
}

} // namespace voip::network
