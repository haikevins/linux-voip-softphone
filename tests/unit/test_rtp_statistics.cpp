#include "network/RtpStatistics.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

namespace voip::network {
namespace {

TEST(RtpStatisticsTest, ReportsZeroLossWhenNoPacketsExist)
{
    const RtpPacketCounts packets{};
    EXPECT_DOUBLE_EQ(calculate_packet_loss_percent(packets), 0.0);
}

TEST(RtpStatisticsTest, CalculatesLossAgainstExpectedPacketCount)
{
    const RtpPacketCounts packets{.received = 986U, .lost = 14U};
    EXPECT_NEAR(calculate_packet_loss_percent(packets), 1.4, 0.0001);
}

TEST(RtpStatisticsTest, AvoidsIntegerOverflowForLargeCounters)
{
    constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
    const RtpPacketCounts packets{.received = maximum - 10U, .lost = 10U};
    EXPECT_GT(calculate_packet_loss_percent(packets), 0.0);
}

TEST(RtpStatisticsTest, BuildsStatisticsSnapshot)
{
    const RtpPacketCounts packets{.received = 4210U, .lost = 14U};
    const auto statistics = make_rtp_statistics(packets, 6.1);

    EXPECT_EQ(statistics.packets_received, 4210U);
    EXPECT_EQ(statistics.packets_lost, 14U);
    EXPECT_NEAR(statistics.packet_loss_percent, 0.3314, 0.0001);
    EXPECT_DOUBLE_EQ(statistics.jitter_ms, 6.1);
}

} // namespace
} // namespace voip::network
