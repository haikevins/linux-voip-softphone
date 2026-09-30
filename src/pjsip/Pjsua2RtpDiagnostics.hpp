#pragma once

#include "network/RtpStatistics.hpp"

#include <pjsua2.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace voip::pjsip {

struct RtpMediaDiagnostics
{
    unsigned media_index{0};
    std::string codec;
    unsigned clock_rate_hz{0};
    unsigned receive_payload_type{0};
    unsigned transmit_payload_type{0};
    std::uint64_t packets_sent{0};
    network::RtpStatistics receive;
    double round_trip_time_ms{0.0};
    std::uint32_t last_transmit_timestamp{0};
    std::uint16_t last_transmit_sequence{0};
};

[[nodiscard]] RtpMediaDiagnostics make_rtp_media_diagnostics(unsigned media_index,
                                                             const pj::StreamInfo &stream_info,
                                                             const pj::StreamStat &stream_stat);

[[nodiscard]] std::vector<RtpMediaDiagnostics> read_active_audio_rtp_diagnostics(pj::Call &call);

} // namespace voip::pjsip
