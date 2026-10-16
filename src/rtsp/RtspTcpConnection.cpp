#include "rtsp/RtspTcpConnection.hpp"

#include "rtsp/RtspParser.hpp"

#include <array>
#include <cerrno>
#include <charconv>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

#include <sys/socket.h>

namespace voip::rtsp {
namespace {

constexpr std::string_view header_terminator = "\r\n\r\n";
constexpr std::size_t maximum_response_size = static_cast<std::size_t>(1024U) * 1024U;

std::string_view trim(std::string_view value) noexcept
{
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
        value.remove_prefix(1);
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
        value.remove_suffix(1);
    }
    return value;
}

bool iequals(std::string_view left, std::string_view right) noexcept
{
    if (left.size() != right.size()) {
        return false;
    }

    for (std::size_t index = 0U; index < left.size(); ++index) {
        auto lhs = left[index];
        auto rhs = right[index];
        if (lhs >= 'A' && lhs <= 'Z') {
            lhs = static_cast<char>(lhs - 'A' + 'a');
        }
        if (rhs >= 'A' && rhs <= 'Z') {
            rhs = static_cast<char>(rhs - 'A' + 'a');
        }
        if (lhs != rhs) {
            return false;
        }
    }
    return true;
}

std::size_t parse_content_length(std::string_view value)
{
    value = trim(value);
    std::size_t parsed = 0U;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) {
        throw std::runtime_error("invalid RTSP Content-Length received from peer");
    }
    return parsed;
}

std::optional<std::size_t> expected_message_size(std::string_view buffer)
{
    const auto header_end = buffer.find(header_terminator);
    if (header_end == std::string_view::npos) {
        return std::nullopt;
    }

    std::size_t content_length = 0U;
    bool found_content_length = false;
    std::size_t line_start = 0U;

    while (line_start < header_end) {
        const auto line_end = buffer.find("\r\n", line_start);
        if (line_end == std::string_view::npos || line_end > header_end) {
            break;
        }

        const auto line = buffer.substr(line_start, line_end - line_start);
        const auto separator = line.find(':');
        if (separator != std::string_view::npos) {
            const auto name = trim(line.substr(0U, separator));
            if (iequals(name, "Content-Length")) {
                if (found_content_length) {
                    throw std::runtime_error("duplicate RTSP Content-Length received from peer");
                }
                content_length = parse_content_length(line.substr(separator + 1U));
                found_content_length = true;
            }
        }

        line_start = line_end + 2U;
    }

    const auto total_size = header_end + header_terminator.size() + content_length;
    if (total_size > maximum_response_size) {
        throw std::length_error("RTSP response exceeds the configured size limit");
    }
    return total_size;
}

void send_all(const int socket_fd, const std::string_view payload)
{
    std::size_t offset = 0U;
    while (offset < payload.size()) {
        const auto sent = ::send(socket_fd, payload.data() + offset, payload.size() - offset, 0);
        if (sent == -1) {
            if (errno == EINTR) {
                continue;
            }
            throw std::system_error(errno, std::generic_category(), "send RTSP request");
        }
        if (sent == 0) {
            throw std::runtime_error("RTSP send made no progress");
        }
        offset += static_cast<std::size_t>(sent);
    }
}

std::string receive_response(const int socket_fd)
{
    std::string buffer;
    buffer.reserve(4096U);
    std::array<char, 4096U> chunk{};

    while (true) {
        if (const auto expected = expected_message_size(buffer); expected.has_value()) {
            if (buffer.size() == expected.value()) {
                return buffer;
            }
            if (buffer.size() > expected.value()) {
                throw std::runtime_error(
                    "pipelined RTSP responses are not supported by this mini-lab");
            }
        }

        const auto received = ::recv(socket_fd, chunk.data(), chunk.size(), 0);
        if (received == -1) {
            if (errno == EINTR) {
                continue;
            }
            throw std::system_error(errno, std::generic_category(), "receive RTSP response");
        }
        if (received == 0) {
            throw std::runtime_error("RTSP peer closed before a complete response arrived");
        }

        buffer.append(chunk.data(), static_cast<std::size_t>(received));
        if (buffer.size() > maximum_response_size) {
            throw std::length_error("RTSP response exceeds the configured size limit");
        }
    }
}

} // namespace

TcpConnection::TcpConnection(network::Ipv4Endpoint endpoint) : socket_(endpoint) {}

void TcpConnection::connect()
{
    socket_.connect();
}

void TcpConnection::disconnect() noexcept
{
    socket_.disconnect();
}

bool TcpConnection::is_connected() const noexcept
{
    return socket_.is_connected();
}

Response TcpConnection::transact(const Request &request)
{
    if (!socket_.is_connected()) {
        throw std::logic_error("RTSP TCP connection is not connected");
    }

    const auto wire_request = request.serialize();
    send_all(socket_.native_handle(), wire_request);
    return parse_response(receive_response(socket_.native_handle()));
}

} // namespace voip::rtsp
