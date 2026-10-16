#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace voip::rtsp {

struct SdpMediaTrack
{
    std::string media;
    std::uint16_t port{0U};
    std::string protocol;
    std::vector<int> payload_types;
    std::optional<std::string> control;
};

struct SdpDescription
{
    std::optional<std::string> session_control;
    std::vector<SdpMediaTrack> media_tracks;
};

struct ControlUriInput
{
    std::string_view base_uri;
    std::string_view control;
};

class SdpError : public std::runtime_error
{
  public:
    using std::runtime_error::runtime_error;
};

[[nodiscard]] SdpDescription parse_sdp(std::string_view body);
[[nodiscard]] std::string resolve_control_uri(const ControlUriInput &input);
[[nodiscard]] const SdpMediaTrack &select_first_rtp_track(const SdpDescription &description);

} // namespace voip::rtsp
