#pragma once

#include <cstdint>

namespace voip::network {

// Named fields prevent receive/loss counters from being swapped at call sites.
struct RtpPacketCounts
{
    std::uint64_t received{0};
    std::uint64_t lost{0};
};

struct RtpStatistics
{
    std::uint64_t packets_received{0};
    std::uint64_t packets_lost{0};
    double packet_loss_percent{0.0};
    double jitter_ms{0.0};
};

[[nodiscard]] double calculate_packet_loss_percent(const RtpPacketCounts &packets) noexcept;

[[nodiscard]] RtpStatistics make_rtp_statistics(const RtpPacketCounts &packets,
                                                double jitter_ms) noexcept;

} // namespace voip::network
