#include "network/UdpSocket.hpp"

#include <cerrno>
#include <stdexcept>
#include <system_error>
#include <utility>

#include <sys/socket.h>

namespace voip::network {

UdpSocket::UdpSocket(Ipv4Endpoint requested_endpoint) : requested_endpoint_(requested_endpoint) {}

void UdpSocket::bind()
{
    if (socket_) {
        return;
    }

    platform::UniqueFd socket{::socket(AF_INET, SOCK_DGRAM, 0)};
    if (!socket) {
        throw std::system_error(errno, std::generic_category(), "create UDP socket");
    }

    constexpr int enable_reuse_address = 1;
    if (::setsockopt(socket.get(), SOL_SOCKET, SO_REUSEADDR, &enable_reuse_address,
                     static_cast<socklen_t>(sizeof(enable_reuse_address))) == -1) {
        throw std::system_error(errno, std::generic_category(), "configure UDP socket");
    }

    const auto requested_address = requested_endpoint_.to_sockaddr();
    if (::bind(socket.get(), reinterpret_cast<const sockaddr *>(&requested_address),
               static_cast<socklen_t>(sizeof(requested_address))) == -1) {
        throw std::system_error(errno, std::generic_category(), "bind UDP socket");
    }

    sockaddr_in local_address{};
    auto local_address_length = static_cast<socklen_t>(sizeof(local_address));
    if (::getsockname(socket.get(), reinterpret_cast<sockaddr *>(&local_address),
                      &local_address_length) == -1) {
        throw std::system_error(errno, std::generic_category(), "query UDP local endpoint");
    }

    local_endpoint_ = Ipv4Endpoint::from_sockaddr(local_address);
    socket_ = std::move(socket);
}

void UdpSocket::close() noexcept
{
    socket_.reset();
    local_endpoint_.reset();
}

bool UdpSocket::is_bound() const noexcept
{
    return static_cast<bool>(socket_);
}

int UdpSocket::native_handle() const noexcept
{
    return socket_.get();
}

Ipv4Endpoint UdpSocket::local_endpoint() const
{
    if (!local_endpoint_.has_value()) {
        throw std::logic_error("UDP socket is not bound");
    }

    return *local_endpoint_;
}

std::size_t UdpSocket::send_to(const std::span<const std::byte> payload,
                               const Ipv4Endpoint &destination) const
{
    if (!socket_) {
        throw std::logic_error("UDP socket is not bound");
    }

    const auto destination_address = destination.to_sockaddr();
    ssize_t send_result = -1;
    do {
        send_result = ::sendto(socket_.get(), payload.data(), payload.size(), 0,
                               reinterpret_cast<const sockaddr *>(&destination_address),
                               static_cast<socklen_t>(sizeof(destination_address)));
    } while (send_result == -1 && errno == EINTR);

    if (send_result == -1) {
        throw std::system_error(errno, std::generic_category(), "send UDP datagram");
    }

    return static_cast<std::size_t>(send_result);
}

UdpReceiveResult UdpSocket::receive_from(const std::span<std::byte> buffer) const
{
    if (!socket_) {
        throw std::logic_error("UDP socket is not bound");
    }

    sockaddr_in peer_address{};
    auto peer_address_length = static_cast<socklen_t>(sizeof(peer_address));
    ssize_t receive_result = -1;
    do {
        receive_result =
            ::recvfrom(socket_.get(), buffer.data(), buffer.size(), 0,
                       reinterpret_cast<sockaddr *>(&peer_address), &peer_address_length);
    } while (receive_result == -1 && errno == EINTR);

    if (receive_result == -1) {
        throw std::system_error(errno, std::generic_category(), "receive UDP datagram");
    }

    return UdpReceiveResult{static_cast<std::size_t>(receive_result),
                            Ipv4Endpoint::from_sockaddr(peer_address)};
}

} // namespace voip::network
