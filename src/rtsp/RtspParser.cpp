#include "rtsp/RtspParser.hpp"

#include <charconv>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace voip::rtsp {
namespace {

constexpr std::string_view header_terminator = "\r\n\r\n";

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

    for (std::size_t index = 0; index < left.size(); ++index) {
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

int parse_positive_integer(std::string_view value, const char *field_name)
{
    value = trim(value);
    int parsed = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size() || parsed <= 0) {
        throw ParseError(std::string{"invalid "} + std::string{field_name});
    }
    return parsed;
}

std::size_t parse_size(std::string_view value, const char *field_name)
{
    value = trim(value);
    std::size_t parsed = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) {
        throw ParseError(std::string{"invalid "} + std::string{field_name});
    }
    return parsed;
}

std::vector<std::string_view> split_lines(std::string_view block)
{
    std::vector<std::string_view> lines;
    while (!block.empty()) {
        const auto end = block.find("\r\n");
        if (end == std::string_view::npos) {
            lines.push_back(block);
            break;
        }
        lines.push_back(block.substr(0, end));
        block.remove_prefix(end + 2);
    }
    return lines;
}

struct ParsedEnvelope
{
    std::string_view start_line;
    std::vector<Header> headers;
    std::string_view body;
    int cseq = 0;
};

ParsedEnvelope parse_envelope(std::string_view message)
{
    const auto boundary = message.find(header_terminator);
    if (boundary == std::string_view::npos) {
        throw ParseError("RTSP message is missing the header terminator");
    }

    const auto header_block = message.substr(0, boundary);
    const auto body = message.substr(boundary + header_terminator.size());
    auto lines = split_lines(header_block);
    if (lines.empty() || lines.front().empty()) {
        throw ParseError("RTSP message is missing a start line");
    }

    ParsedEnvelope envelope;
    envelope.start_line = lines.front();
    envelope.body = body;

    std::optional<std::size_t> content_length;
    bool found_cseq = false;

    for (std::size_t index = 1; index < lines.size(); ++index) {
        const auto line = lines[index];
        if (line.empty()) {
            throw ParseError("unexpected empty RTSP header line");
        }

        const auto separator = line.find(':');
        if (separator == std::string_view::npos || separator == 0) {
            throw ParseError("malformed RTSP header");
        }

        const auto name = trim(line.substr(0, separator));
        const auto value = trim(line.substr(separator + 1));
        if (name.empty()) {
            throw ParseError("RTSP header name must not be empty");
        }

        if (iequals(name, "CSeq")) {
            if (found_cseq) {
                throw ParseError("duplicate CSeq header");
            }
            envelope.cseq = parse_positive_integer(value, "CSeq header");
            found_cseq = true;
            continue;
        }

        if (iequals(name, "Content-Length")) {
            if (content_length.has_value()) {
                throw ParseError("duplicate Content-Length header");
            }
            content_length = parse_size(value, "Content-Length header");
            continue;
        }

        envelope.headers.push_back(Header{std::string{name}, std::string{value}});
    }

    if (!found_cseq) {
        throw ParseError("RTSP message is missing CSeq");
    }

    if (content_length.has_value()) {
        if (*content_length != body.size()) {
            throw ParseError("RTSP Content-Length does not match the message body");
        }
    } else if (!body.empty()) {
        throw ParseError("RTSP body requires Content-Length");
    }

    return envelope;
}

void copy_headers(const std::vector<Header> &headers, Request &request)
{
    for (const auto &header : headers) {
        request.add_header(header.name, header.value);
    }
}

void copy_headers(const std::vector<Header> &headers, Response &response)
{
    for (const auto &header : headers) {
        response.add_header(header.name, header.value);
    }
}

} // namespace

Request parse_request(std::string_view message)
{
    auto envelope = parse_envelope(message);

    const auto first_space = envelope.start_line.find(' ');
    const auto second_space = envelope.start_line.find(' ', first_space + 1);
    if (first_space == std::string_view::npos || second_space == std::string_view::npos) {
        throw ParseError("malformed RTSP request line");
    }

    const auto method_token = envelope.start_line.substr(0, first_space);
    const auto uri = envelope.start_line.substr(first_space + 1, second_space - first_space - 1);
    const auto version = envelope.start_line.substr(second_space + 1);

    const auto method = parse_method(method_token);
    if (!method.has_value()) {
        throw ParseError("unsupported RTSP method");
    }
    if (uri.empty()) {
        throw ParseError("RTSP request URI must not be empty");
    }
    if (version != "RTSP/1.0") {
        throw ParseError("unsupported RTSP version");
    }

    Request request{*method, std::string{uri}, envelope.cseq};
    copy_headers(envelope.headers, request);
    request.set_body(std::string{envelope.body});
    return request;
}

Response parse_response(std::string_view message)
{
    auto envelope = parse_envelope(message);

    constexpr std::string_view prefix = "RTSP/1.0 ";
    if (!envelope.start_line.starts_with(prefix)) {
        throw ParseError("unsupported RTSP response version");
    }

    auto remainder = envelope.start_line.substr(prefix.size());
    const auto separator = remainder.find(' ');
    if (separator == std::string_view::npos) {
        throw ParseError("malformed RTSP status line");
    }

    const auto status_token = remainder.substr(0, separator);
    if (status_token.size() != 3) {
        throw ParseError("RTSP status code must contain three digits");
    }

    int status_code = 0;
    const auto status_result = std::from_chars(
        status_token.data(), status_token.data() + status_token.size(), status_code);
    if (status_result.ec != std::errc{} ||
        status_result.ptr != status_token.data() + status_token.size() || status_code < 100 ||
        status_code > 999) {
        throw ParseError("invalid RTSP status code");
    }

    const auto reason = remainder.substr(separator + 1);
    Response response{status_code, std::string{reason}, envelope.cseq};
    copy_headers(envelope.headers, response);
    response.set_body(std::string{envelope.body});
    return response;
}

} // namespace voip::rtsp
