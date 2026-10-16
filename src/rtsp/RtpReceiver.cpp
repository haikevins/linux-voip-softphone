#include "rtsp/RtpReceiver.hpp"

#include <array>
#include <cstddef>

namespace voip::rtsp {

void RtpStreamTracker::observe(const RtpPacketHeader &header) noexcept
{
    ++received_;
    if (!highest_sequence_.has_value()) {
        highest_sequence_ = header.sequence_number;
        return;
    }

    const auto highest = highest_sequence_.value_or(0U);
    const auto delta = static_cast<std::uint16_t>(header.sequence_number - highest);
    if (delta == 0U || delta >= 0x8000U) {
        return;
    }

    if (delta > 1U) {
        lost_ += static_cast<std::uint64_t>(delta - 1U);
    }
    highest_sequence_ = header.sequence_number;
}

network::RtpStatistics RtpStreamTracker::statistics() const noexcept
{
    return network::make_rtp_statistics(
        network::RtpPacketCounts{.received = received_, .lost = lost_}, 0.0);
}

RtpReceiver::RtpReceiver(network::Ipv4Endpoint local_endpoint) : socket_(local_endpoint) {}

void RtpReceiver::bind()
{
    socket_.bind();
}

void RtpReceiver::close() noexcept
{
    socket_.close();
}

bool RtpReceiver::is_bound() const noexcept
{
    return socket_.is_bound();
}

network::Ipv4Endpoint RtpReceiver::local_endpoint() const
{
    return socket_.local_endpoint();
}

RtpReceiveResult RtpReceiver::receive_one()
{
    std::array<std::byte, 65536U> buffer{};
    const auto received = socket_.receive_from(buffer);
    const auto packet = std::span<const std::byte>{buffer.data(), received.bytes_received};
    const auto header = parse_rtp_packet_header(packet);
    tracker_.observe(header);
    return RtpReceiveResult{.header = header,
                            .peer_endpoint = received.peer_endpoint,
                            .statistics = tracker_.statistics()};
}

network::RtpStatistics RtpReceiver::statistics() const noexcept
{
    return tracker_.statistics();
}

} // namespace voip::rtsp
