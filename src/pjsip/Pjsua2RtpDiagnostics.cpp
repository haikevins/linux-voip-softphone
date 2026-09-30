#include "pjsip/Pjsua2RtpDiagnostics.hpp"

#include "pjsip/Pjsua2CallAudioBridge.hpp"

#include <algorithm>
#include <stdexcept>

namespace voip::pjsip {
namespace {

[[nodiscard]] double microseconds_to_milliseconds(const int microseconds) noexcept
{
    return static_cast<double>(std::max(microseconds, 0)) / 1000.0;
}

} // namespace

RtpMediaDiagnostics make_rtp_media_diagnostics(const unsigned media_index,
                                               const pj::StreamInfo &stream_info,
                                               const pj::StreamStat &stream_stat)
{
    if (stream_info.type != PJMEDIA_TYPE_AUDIO) {
        throw std::invalid_argument("RTP diagnostics require an audio media stream");
    }

    const auto &receive_stat = stream_stat.rtcp.rxStat;
    const auto &transmit_stat = stream_stat.rtcp.txStat;

    return RtpMediaDiagnostics{
        .media_index = media_index,
        .codec = stream_info.codecName,
        .clock_rate_hz = stream_info.codecClockRate,
        .receive_payload_type = stream_info.rxPt,
        .transmit_payload_type = stream_info.txPt,
        .packets_sent = static_cast<std::uint64_t>(transmit_stat.pkt),
        .receive = network::make_rtp_statistics(
            network::RtpPacketCounts{
                .received = static_cast<std::uint64_t>(receive_stat.pkt),
                .lost = static_cast<std::uint64_t>(receive_stat.loss),
            },
            microseconds_to_milliseconds(receive_stat.jitterUsec.mean)),
        .round_trip_time_ms = microseconds_to_milliseconds(stream_stat.rtcp.rttUsec.mean),
        .last_transmit_timestamp = static_cast<std::uint32_t>(stream_stat.rtcp.rtpTxLastTs),
        .last_transmit_sequence = static_cast<std::uint16_t>(stream_stat.rtcp.rtpTxLastSeq)};
}

std::vector<RtpMediaDiagnostics> read_active_audio_rtp_diagnostics(pj::Call &call)
{
    const auto call_info = call.getInfo();
    const auto media_indices = active_audio_media_indices(call_info);

    std::vector<RtpMediaDiagnostics> diagnostics;
    diagnostics.reserve(media_indices.size());

    for (const auto media_index : media_indices) {
        diagnostics.push_back(make_rtp_media_diagnostics(
            media_index, call.getStreamInfo(media_index), call.getStreamStat(media_index)));
    }

    return diagnostics;
}

} // namespace voip::pjsip
