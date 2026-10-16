#pragma once

#include "network/Ipv4Endpoint.hpp"
#include "network/TcpClientSocket.hpp"
#include "rtsp/RtspConnection.hpp"

namespace voip::rtsp {

class TcpConnection final : public Connection
{
  public:
    explicit TcpConnection(network::Ipv4Endpoint endpoint);

    void connect() override;
    void disconnect() noexcept override;
    [[nodiscard]] bool is_connected() const noexcept override;
    [[nodiscard]] Response transact(const Request &request) override;

  private:
    network::TcpClientSocket socket_;
};

} // namespace voip::rtsp
