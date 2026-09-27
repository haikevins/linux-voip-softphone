#pragma once

#include <cstdint>
#include <string>

#include <netinet/in.h>

namespace voip::network {

class Ipv4Endpoint
{
  public:
    Ipv4Endpoint(const std::string &address, std::uint16_t port);

    [[nodiscard]] static Ipv4Endpoint from_sockaddr(const sockaddr_in &address);

    [[nodiscard]] std::string address() const;
    [[nodiscard]] std::uint16_t port() const noexcept;
    [[nodiscard]] sockaddr_in to_sockaddr() const noexcept;

    [[nodiscard]] bool operator==(const Ipv4Endpoint &other) const noexcept;

  private:
    Ipv4Endpoint(in_addr address, std::uint16_t port) noexcept;

    in_addr address_{};
    std::uint16_t port_{0U};
};

} // namespace voip::network
