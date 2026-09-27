#pragma once

#include "network/Ipv4Endpoint.hpp"
#include "platform/linux/UniqueFd.hpp"

#include <optional>

namespace voip::network {

class TcpServerSocket
{
  public:
    explicit TcpServerSocket(Ipv4Endpoint endpoint);

    TcpServerSocket(const TcpServerSocket &) = delete;
    TcpServerSocket &operator=(const TcpServerSocket &) = delete;
    TcpServerSocket(TcpServerSocket &&) = delete;
    TcpServerSocket &operator=(TcpServerSocket &&) = delete;

    void start();
    void stop() noexcept;

    [[nodiscard]] platform::UniqueFd accept_one() const;
    [[nodiscard]] bool is_listening() const noexcept;
    [[nodiscard]] Ipv4Endpoint local_endpoint() const;

  private:
    Ipv4Endpoint requested_endpoint_;
    std::optional<Ipv4Endpoint> local_endpoint_;
    platform::UniqueFd listener_;
};

} // namespace voip::network
