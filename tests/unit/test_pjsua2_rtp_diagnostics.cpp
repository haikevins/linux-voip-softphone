#include "pjsip/Pjsua2RtpDiagnostics.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

namespace voip::pjsip {
namespace {

TEST(Pjsua2RtpDiagnosticsTest, ConvertsPjsua2AudioStreamStatistics)
{
    pj::StreamInfo stream_info{};
    stream_info.type = PJMEDIA_TYPE_AUDIO;
    stream_info.codecName = "PCMU";
    stream_info.codecClockRate = 8000U;
    stream_info.rxPt = 0U;
    stream_info.txPt = 0U;

    pj::StreamStat stream_stat{};
    stream_stat.rtcp.rxStat.pkt = 4210U;
    stream_stat.rtcp.rxStat.loss = 14U;
    stream_stat.rtcp.rxStat.jitterUsec.mean = 6100;
    stream_stat.rtcp.txStat.pkt = 4198U;
    stream_stat.rtcp.rttUsec.mean = 737;
    stream_stat.rtcp.rtpTxLastTs = 123456U;
    stream_stat.rtcp.rtpTxLastSeq = 321U;

    const auto diagnostics = make_rtp_media_diagnostics(2U, stream_info, stream_stat);

    EXPECT_EQ(diagnostics.media_index, 2U);
    EXPECT_EQ(diagnostics.codec, "PCMU");
    EXPECT_EQ(diagnostics.clock_rate_hz, 8000U);
    EXPECT_EQ(diagnostics.receive_payload_type, 0U);
    EXPECT_EQ(diagnostics.transmit_payload_type, 0U);
    EXPECT_EQ(diagnostics.packets_sent, 4198U);
    EXPECT_EQ(diagnostics.receive.packets_received, 4210U);
    EXPECT_EQ(diagnostics.receive.packets_lost, 14U);
    EXPECT_NEAR(diagnostics.receive.packet_loss_percent, 0.3314, 0.0001);
    EXPECT_DOUBLE_EQ(diagnostics.receive.jitter_ms, 6.1);
    EXPECT_DOUBLE_EQ(diagnostics.round_trip_time_ms, 0.737);
    EXPECT_EQ(diagnostics.last_transmit_timestamp, 123456U);
    EXPECT_EQ(diagnostics.last_transmit_sequence, 321U);
}

TEST(Pjsua2RtpDiagnosticsTest, ClampsUnavailableTimingStatisticsToZero)
{
    pj::StreamInfo stream_info{};
    stream_info.type = PJMEDIA_TYPE_AUDIO;

    pj::StreamStat stream_stat{};
    stream_stat.rtcp.rxStat.pkt = 0U;
    stream_stat.rtcp.rxStat.loss = 0U;
    stream_stat.rtcp.rxStat.jitterUsec.mean = -1;
    stream_stat.rtcp.txStat.pkt = 0U;
    stream_stat.rtcp.rttUsec.mean = -1;
    stream_stat.rtcp.rtpTxLastTs = 0U;
    stream_stat.rtcp.rtpTxLastSeq = 0U;

    const auto diagnostics = make_rtp_media_diagnostics(0U, stream_info, stream_stat);

    EXPECT_DOUBLE_EQ(diagnostics.receive.jitter_ms, 0.0);
    EXPECT_DOUBLE_EQ(diagnostics.round_trip_time_ms, 0.0);
}

TEST(Pjsua2RtpDiagnosticsTest, RejectsNonAudioStreams)
{
    pj::StreamInfo stream_info{};
    stream_info.type = PJMEDIA_TYPE_VIDEO;
    const pj::StreamStat stream_stat{};

    EXPECT_THROW(static_cast<void>(make_rtp_media_diagnostics(0U, stream_info, stream_stat)),
                 std::invalid_argument);
}

} // namespace
} // namespace voip::pjsip
