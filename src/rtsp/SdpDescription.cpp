#include "rtsp/SdpDescription.hpp"

#include <charconv>
#include <cstddef>
#include <limits>
#include <sstream>
#include <utility>

namespace voip::rtsp {
namespace {

std::string_view trim_carriage_return(std::string_view line)
{
    if (!line.empty() && line.back() == '\r') {
        line.remove_suffix(1U);
    }
    return line;
}

std::uint16_t parse_port(const std::string_view token)
{
    unsigned int value = 0U;
    const auto result = std::from_chars(token.data(), token.data() + token.size(), value);
    if (result.ec != std::errc{} || result.ptr != token.data() + token.size() || value == 0U ||
        value > std::numeric_limits<std::uint16_t>::max()) {
        throw SdpError("SDP media port is invalid");
    }
    return static_cast<std::uint16_t>(value);
}

int parse_payload_type(const std::string_view token)
{
    int value = -1;
    const auto result = std::from_chars(token.data(), token.data() + token.size(), value);
    if (result.ec != std::errc{} || result.ptr != token.data() + token.size() || value < 0 ||
        value > 127) {
        throw SdpError("SDP RTP payload type is invalid");
    }
    return value;
}

SdpMediaTrack parse_media_line(const std::string_view value)
{
    std::istringstream input{std::string{value}};
    std::string media;
    std::string port_token;
    std::string protocol;
    if (!(input >> media >> port_token >> protocol)) {
        throw SdpError("SDP media line is incomplete");
    }

    SdpMediaTrack track{.media = std::move(media),
                        .port = parse_port(port_token),
                        .protocol = std::move(protocol),
                        .payload_types = {},
                        .control = std::nullopt};

    std::string payload_token;
    while (input >> payload_token) {
        track.payload_types.push_back(parse_payload_type(payload_token));
    }
    if (track.payload_types.empty()) {
        throw SdpError("SDP media line has no payload type");
    }
    return track;
}

bool is_absolute_rtsp_uri(const std::string_view value)
{
    return value.starts_with("rtsp://") || value.starts_with("rtsps://");
}

} // namespace

SdpDescription parse_sdp(const std::string_view body)
{
    if (body.empty()) {
        throw SdpError("SDP body is empty");
    }

    SdpDescription description;
    SdpMediaTrack *current_track = nullptr;

    std::size_t offset = 0U;
    while (offset <= body.size()) {
        const auto line_end = body.find('\n', offset);
        const auto raw_line = body.substr(
            offset, line_end == std::string_view::npos ? body.size() - offset : line_end - offset);
        const auto line = trim_carriage_return(raw_line);

        if (!line.empty()) {
            if (line.size() < 2U || line[1] != '=') {
                throw SdpError("SDP line is malformed");
            }

            const auto value = line.substr(2U);
            if (line[0] == 'm') {
                description.media_tracks.push_back(parse_media_line(value));
                current_track = &description.media_tracks.back();
            } else if (line[0] == 'a' && value.starts_with("control:")) {
                auto control = std::string{value.substr(std::string_view{"control:"}.size())};
                if (control.empty()) {
                    throw SdpError("SDP control attribute is empty");
                }
                if (current_track != nullptr) {
                    current_track->control = std::move(control);
                } else {
                    description.session_control = std::move(control);
                }
            }
        }

        if (line_end == std::string_view::npos) {
            break;
        }
        offset = line_end + 1U;
    }

    if (description.media_tracks.empty()) {
        throw SdpError("SDP contains no media tracks");
    }
    return description;
}

std::string resolve_control_uri(const ControlUriInput &input)
{
    if (!is_absolute_rtsp_uri(input.base_uri)) {
        throw std::invalid_argument("RTSP base URI must be absolute");
    }
    if (input.control.empty() || input.control == "*") {
        return std::string{input.base_uri};
    }
    if (is_absolute_rtsp_uri(input.control)) {
        return std::string{input.control};
    }

    std::string result{input.base_uri};
    if (!result.ends_with('/')) {
        result.push_back('/');
    }
    auto control = input.control;
    if (control.starts_with('/')) {
        control.remove_prefix(1U);
    }
    result.append(control);
    return result;
}

const SdpMediaTrack &select_first_rtp_track(const SdpDescription &description)
{
    for (const auto &track : description.media_tracks) {
        if (track.protocol == "RTP/AVP" || track.protocol == "RTP/AVP/UDP") {
            return track;
        }
    }
    throw SdpError("SDP contains no RTP/AVP media track");
}

} // namespace voip::rtsp
