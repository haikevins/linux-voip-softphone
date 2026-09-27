#include "network/Ipv4Endpoint.hpp"

#include <array>
#include <cerrno>
#include <stdexcept>
#include <system_error>

#include <arpa/inet.h>
#include <sys/socket.h>

namespace voip::network {

Ipv4Endpoint::Ipv4Endpoint(const std::string &address, const std::uint16_t port) : port_(port)
{
    if (address.empty()) {
        throw std::invalid_argument("IPv4 address must not be empty");
    }

    const auto parse_result = ::inet_pton(AF_INET, address.c_str(), &address_);
    if (parse_result == 0) {
        throw std::invalid_argument("invalid IPv4 address: " + address);
    }
    if (parse_result == -1) {
        throw std::system_error(errno, std::generic_category(), "parse IPv4 address");
    }
}

Ipv4Endpoint::Ipv4Endpoint(const in_addr address, const std::uint16_t port) noexcept
    : address_(address), port_(port)
{}

Ipv4Endpoint Ipv4Endpoint::from_sockaddr(const sockaddr_in &address)
{
    if (address.sin_family != AF_INET) {
        throw std::invalid_argument("sockaddr does not contain an IPv4 address");
    }

    return Ipv4Endpoint{address.sin_addr, ntohs(address.sin_port)};
}

std::string Ipv4Endpoint::address() const
{
    std::array<char, INET_ADDRSTRLEN> buffer{};
    const auto *result =
        ::inet_ntop(AF_INET, &address_, buffer.data(), static_cast<socklen_t>(buffer.size()));
    if (result == nullptr) {
        throw std::system_error(errno, std::generic_category(), "format IPv4 address");
    }

    return std::string{result};
}

std::uint16_t Ipv4Endpoint::port() const noexcept
{
    return port_;
}

sockaddr_in Ipv4Endpoint::to_sockaddr() const noexcept
{
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port_);
    address.sin_addr = address_;
    return address;
}

bool Ipv4Endpoint::operator==(const Ipv4Endpoint &other) const noexcept
{
    return address_.s_addr == other.address_.s_addr && port_ == other.port_;
}

} // namespace voip::network
