#pragma once

#include "rtsp/RtspMessage.hpp"

#include <stdexcept>
#include <string_view>

namespace voip::rtsp {

class ParseError : public std::runtime_error
{
  public:
    using std::runtime_error::runtime_error;
};

[[nodiscard]] Request parse_request(std::string_view message);
[[nodiscard]] Response parse_response(std::string_view message);

} // namespace voip::rtsp
