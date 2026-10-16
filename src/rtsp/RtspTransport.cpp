#include "rtsp/RtspTransport.hpp"

#include <charconv>
#include <limits>
#include <string>

namespace voip::rtsp {
namespace {

std::uint16_t parse_port(const std::string_view token)
{
    unsigned int value = 0U;
    const auto result = std::from_chars(token.data(), token.data() + token.size(), value);
    if (result.ec != std::errc{} || result.ptr != token.data() + token.size() || value == 0U ||
        value > std::numeric_limits<std::uint16_t>::max()) {
        throw TransportError("RTSP transport contains an invalid UDP port");
    }
    return static_cast<std::uint16_t>(value);
}

PortPair parse_port_pair(const std::string_view value)
{
    const auto separator = value.find('-');
    if (separator == std::string_view::npos) {
        throw TransportError("RTSP transport port pair is missing '-'");
    }
    return PortPair{.rtp = parse_port(value.substr(0U, separator)),
                    .rtcp = parse_port(value.substr(separator + 1U))};
}

} // namespace

TransportInfo parse_transport_header(const std::string_view value)
{
    if (value.empty()) {
        throw TransportError("RTSP Transport header is empty");
    }

    const auto first_separator = value.find(';');
    const auto profile = value.substr(0U, first_separator);
    if (profile != "RTP/AVP" && profile != "RTP/AVP/UDP") {
        throw TransportError("RTSP Transport header is not RTP over UDP");
    }

    TransportInfo info;
    std::size_t offset =
        first_separator == std::string_view::npos ? value.size() : first_separator + 1U;
    while (offset < value.size()) {
        const auto separator = value.find(';', offset);
        const auto token =
            value.substr(offset, separator == std::string_view::npos ? value.size() - offset
                                                                     : separator - offset);

        if (token == "unicast") {
            info.unicast = true;
        } else if (token.starts_with("client_port=")) {
            info.client_ports =
                parse_port_pair(token.substr(std::string_view{"client_port="}.size()));
        } else if (token.starts_with("server_port=")) {
            info.server_ports =
                parse_port_pair(token.substr(std::string_view{"server_port="}.size()));
        }

        if (separator == std::string_view::npos) {
            break;
        }
        offset = separator + 1U;
    }

    if (!info.unicast) {
        throw TransportError("RTSP Transport header must select unicast RTP");
    }
    return info;
}

} // namespace voip::rtsp
