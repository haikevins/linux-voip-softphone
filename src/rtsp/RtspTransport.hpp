#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace voip::rtsp {

struct PortPair
{
    std::uint16_t rtp{0U};
    std::uint16_t rtcp{0U};
};

struct TransportInfo
{
    bool unicast{false};
    std::optional<PortPair> client_ports;
    std::optional<PortPair> server_ports;
};

class TransportError : public std::runtime_error
{
  public:
    using std::runtime_error::runtime_error;
};

[[nodiscard]] TransportInfo parse_transport_header(std::string_view value);

} // namespace voip::rtsp
