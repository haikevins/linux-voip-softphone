#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>

namespace voip::rtsp {

struct RtpPacketHeader
{
    bool marker{false};
    std::uint8_t payload_type{0U};
    std::uint16_t sequence_number{0U};
    std::uint32_t timestamp{0U};
    std::uint32_t ssrc{0U};
    std::size_t header_size{0U};
};

class RtpPacketError : public std::runtime_error
{
  public:
    using std::runtime_error::runtime_error;
};

[[nodiscard]] RtpPacketHeader parse_rtp_packet_header(std::span<const std::byte> packet);

} // namespace voip::rtsp
