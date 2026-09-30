#include "network/RtpStatistics.hpp"

namespace voip::network {

double calculate_packet_loss_percent(const RtpPacketCounts &packets) noexcept
{
    const auto received = static_cast<long double>(packets.received);
    const auto lost = static_cast<long double>(packets.lost);
    const auto expected = received + lost;
    if (expected == 0.0L) {
        return 0.0;
    }

    return static_cast<double>((lost * 100.0L) / expected);
}

RtpStatistics make_rtp_statistics(const RtpPacketCounts &packets, const double jitter_ms) noexcept
{
    return RtpStatistics{.packets_received = packets.received,
                         .packets_lost = packets.lost,
                         .packet_loss_percent = calculate_packet_loss_percent(packets),
                         .jitter_ms = jitter_ms};
}

} // namespace voip::network
