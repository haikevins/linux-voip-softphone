#pragma once

#include "network/Ipv4Endpoint.hpp"
#include "platform/linux/UniqueFd.hpp"

#include <cstddef>
#include <optional>
#include <span>

namespace voip::network {

struct UdpReceiveResult
{
    std::size_t bytes_received;
    Ipv4Endpoint peer_endpoint;
};

class UdpSocket
{
  public:
    explicit UdpSocket(Ipv4Endpoint requested_endpoint);

    UdpSocket(const UdpSocket &) = delete;
    UdpSocket &operator=(const UdpSocket &) = delete;
    UdpSocket(UdpSocket &&) = delete;
    UdpSocket &operator=(UdpSocket &&) = delete;

    void bind();
    void close() noexcept;

    [[nodiscard]] bool is_bound() const noexcept;
    [[nodiscard]] int native_handle() const noexcept;
    [[nodiscard]] Ipv4Endpoint local_endpoint() const;

    [[nodiscard]] std::size_t send_to(std::span<const std::byte> payload,
                                      const Ipv4Endpoint &destination) const;
    [[nodiscard]] UdpReceiveResult receive_from(std::span<std::byte> buffer) const;

  private:
    Ipv4Endpoint requested_endpoint_;
    std::optional<Ipv4Endpoint> local_endpoint_;
    platform::UniqueFd socket_;
};

} // namespace voip::network
