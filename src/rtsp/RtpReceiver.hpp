#pragma once

#include "network/Ipv4Endpoint.hpp"
#include "network/RtpStatistics.hpp"
#include "network/UdpSocket.hpp"
#include "rtsp/RtpPacket.hpp"

#include <cstdint>
#include <optional>

namespace voip::rtsp {

class RtpStreamTracker
{
  public:
    void observe(const RtpPacketHeader &header) noexcept;
    [[nodiscard]] network::RtpStatistics statistics() const noexcept;

  private:
    std::uint64_t received_{0U};
    std::uint64_t lost_{0U};
    std::optional<std::uint16_t> highest_sequence_;
};

struct RtpReceiveResult
{
    RtpPacketHeader header;
    network::Ipv4Endpoint peer_endpoint;
    network::RtpStatistics statistics;
};

class RtpReceiver
{
  public:
    explicit RtpReceiver(network::Ipv4Endpoint local_endpoint);

    void bind();
    void close() noexcept;

    [[nodiscard]] bool is_bound() const noexcept;
    [[nodiscard]] network::Ipv4Endpoint local_endpoint() const;
    [[nodiscard]] RtpReceiveResult receive_one();
    [[nodiscard]] network::RtpStatistics statistics() const noexcept;

  private:
    network::UdpSocket socket_;
    RtpStreamTracker tracker_;
};

} // namespace voip::rtsp
