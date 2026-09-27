#pragma once

#include "network/Ipv4Endpoint.hpp"
#include "platform/linux/UniqueFd.hpp"

#include <optional>

namespace voip::network {

class TcpClientSocket
{
  public:
    explicit TcpClientSocket(Ipv4Endpoint remote_endpoint);

    TcpClientSocket(const TcpClientSocket &) = delete;
    TcpClientSocket &operator=(const TcpClientSocket &) = delete;
    TcpClientSocket(TcpClientSocket &&) = delete;
    TcpClientSocket &operator=(TcpClientSocket &&) = delete;

    void connect();
    void disconnect() noexcept;

    [[nodiscard]] bool is_connected() const noexcept;
    [[nodiscard]] int native_handle() const noexcept;
    [[nodiscard]] const Ipv4Endpoint &remote_endpoint() const noexcept;
    [[nodiscard]] Ipv4Endpoint local_endpoint() const;

  private:
    Ipv4Endpoint remote_endpoint_;
    std::optional<Ipv4Endpoint> local_endpoint_;
    platform::UniqueFd socket_;
};

} // namespace voip::network
