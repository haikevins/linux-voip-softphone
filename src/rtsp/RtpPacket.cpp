#include "rtsp/RtpPacket.hpp"

#include <array>
#include <limits>

namespace voip::rtsp {
namespace {

constexpr std::size_t fixed_header_size = 12U;
constexpr std::size_t csrc_size = 4U;
constexpr std::size_t extension_header_size = 4U;

std::uint16_t read_u16(const std::span<const std::byte> packet, const std::size_t offset)
{
    return static_cast<std::uint16_t>((std::to_integer<std::uint16_t>(packet[offset]) << 8U) |
                                      std::to_integer<std::uint16_t>(packet[offset + 1U]));
}

std::uint32_t read_u32(const std::span<const std::byte> packet, const std::size_t offset)
{
    return (std::to_integer<std::uint32_t>(packet[offset]) << 24U) |
           (std::to_integer<std::uint32_t>(packet[offset + 1U]) << 16U) |
           (std::to_integer<std::uint32_t>(packet[offset + 2U]) << 8U) |
           std::to_integer<std::uint32_t>(packet[offset + 3U]);
}

} // namespace

RtpPacketHeader parse_rtp_packet_header(const std::span<const std::byte> packet)
{
    if (packet.size() < fixed_header_size) {
        throw RtpPacketError("RTP packet is shorter than the fixed header");
    }

    const auto first = std::to_integer<unsigned int>(packet[0]);
    const auto second = std::to_integer<unsigned int>(packet[1]);
    const auto version = (first >> 6U) & 0x03U;
    if (version != 2U) {
        throw RtpPacketError("RTP packet version is not 2");
    }

    const bool has_extension = (first & 0x10U) != 0U;
    const auto csrc_count = static_cast<std::size_t>(first & 0x0FU);
    std::size_t header_size = fixed_header_size + (csrc_count * csrc_size);
    if (header_size > packet.size()) {
        throw RtpPacketError("RTP packet has a truncated CSRC list");
    }

    if (has_extension) {
        if (packet.size() - header_size < extension_header_size) {
            throw RtpPacketError("RTP packet has a truncated extension header");
        }
        const auto extension_words = static_cast<std::size_t>(read_u16(packet, header_size + 2U));
        if (extension_words >
            (std::numeric_limits<std::size_t>::max() - extension_header_size) / 4U) {
            throw RtpPacketError("RTP extension length is too large");
        }
        const auto extension_size = extension_header_size + (extension_words * 4U);
        if (extension_size > packet.size() - header_size) {
            throw RtpPacketError("RTP packet has truncated extension data");
        }
        header_size += extension_size;
    }

    return RtpPacketHeader{.marker = (second & 0x80U) != 0U,
                           .payload_type = static_cast<std::uint8_t>(second & 0x7FU),
                           .sequence_number = read_u16(packet, 2U),
                           .timestamp = read_u32(packet, 4U),
                           .ssrc = read_u32(packet, 8U),
                           .header_size = header_size};
}

} // namespace voip::rtsp
