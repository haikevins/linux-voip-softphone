#include "rtsp/RtspMessage.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <utility>

namespace voip::rtsp {
namespace {

bool iequals(std::string_view left, std::string_view right) noexcept
{
    if (left.size() != right.size()) {
        return false;
    }

    return std::equal(left.begin(), left.end(), right.begin(), [](char lhs, char rhs) {
        const auto lhs_value = static_cast<unsigned char>(lhs);
        const auto rhs_value = static_cast<unsigned char>(rhs);
        return std::tolower(lhs_value) == std::tolower(rhs_value);
    });
}

std::optional<std::string_view> find_header(const std::vector<Header> &headers,
                                            std::string_view name) noexcept
{
    const auto match = std::find_if(headers.begin(), headers.end(), [name](const Header &header) {
        return iequals(header.name, name);
    });

    if (match == headers.end()) {
        return std::nullopt;
    }

    return match->value;
}

void validate_cseq(int cseq)
{
    if (cseq <= 0) {
        throw std::invalid_argument("RTSP CSeq must be positive");
    }
}

void validate_header_name(std::string_view name)
{
    if (name.empty() || name.find_first_of(":\r\n") != std::string_view::npos) {
        throw std::invalid_argument("invalid RTSP header name");
    }
}

void validate_header_value(std::string_view value)
{
    if (value.find_first_of("\r\n") != std::string_view::npos) {
        throw std::invalid_argument("invalid RTSP header value");
    }
}

void append_headers(std::string &message, const std::vector<Header> &headers)
{
    for (const auto &header : headers) {
        if (iequals(header.name, "CSeq") || iequals(header.name, "Content-Length")) {
            continue;
        }
        message += header.name;
        message += ": ";
        message += header.value;
        message += "\r\n";
    }
}

void append_body(std::string &message, const std::string &body)
{
    if (!body.empty()) {
        message += "Content-Length: ";
        message += std::to_string(body.size());
        message += "\r\n";
    }

    message += "\r\n";
    message += body;
}

} // namespace

std::string_view to_string(Method method) noexcept
{
    switch (method) {
    case Method::Options:
        return "OPTIONS";
    case Method::Describe:
        return "DESCRIBE";
    case Method::Setup:
        return "SETUP";
    case Method::Play:
        return "PLAY";
    case Method::Teardown:
        return "TEARDOWN";
    }

    return "";
}

std::optional<Method> parse_method(std::string_view token) noexcept
{
    if (token == "OPTIONS") {
        return Method::Options;
    }
    if (token == "DESCRIBE") {
        return Method::Describe;
    }
    if (token == "SETUP") {
        return Method::Setup;
    }
    if (token == "PLAY") {
        return Method::Play;
    }
    if (token == "TEARDOWN") {
        return Method::Teardown;
    }
    return std::nullopt;
}

Request::Request(Method method, std::string uri, int cseq)
    : method_(method), uri_(std::move(uri)), cseq_(cseq)
{
    if (uri_.empty()) {
        throw std::invalid_argument("RTSP request URI must not be empty");
    }
    validate_cseq(cseq_);
}

Method Request::method() const noexcept
{
    return method_;
}

const std::string &Request::uri() const noexcept
{
    return uri_;
}

int Request::cseq() const noexcept
{
    return cseq_;
}

const std::vector<Header> &Request::headers() const noexcept
{
    return headers_;
}

const std::string &Request::body() const noexcept
{
    return body_;
}

std::optional<std::string_view> Request::header(std::string_view name) const noexcept
{
    return find_header(headers_, name);
}

void Request::add_header(std::string name, std::string value)
{
    validate_header_name(name);
    validate_header_value(value);
    headers_.push_back(Header{std::move(name), std::move(value)});
}

void Request::set_body(std::string body)
{
    body_ = std::move(body);
}

std::string Request::serialize() const
{
    std::string message;
    message += to_string(method_);
    message += ' ';
    message += uri_;
    message += " RTSP/1.0\r\nCSeq: ";
    message += std::to_string(cseq_);
    message += "\r\n";
    append_headers(message, headers_);
    append_body(message, body_);
    return message;
}

Response::Response(int status_code, std::string reason, int cseq)
    : status_code_(status_code), reason_(std::move(reason)), cseq_(cseq)
{
    if (status_code_ < 100 || status_code_ > 999) {
        throw std::invalid_argument("RTSP status code must contain three digits");
    }
    if (reason_.find_first_of("\r\n") != std::string::npos) {
        throw std::invalid_argument("invalid RTSP reason phrase");
    }
    validate_cseq(cseq_);
}

int Response::status_code() const noexcept
{
    return status_code_;
}

const std::string &Response::reason() const noexcept
{
    return reason_;
}

int Response::cseq() const noexcept
{
    return cseq_;
}

const std::vector<Header> &Response::headers() const noexcept
{
    return headers_;
}

const std::string &Response::body() const noexcept
{
    return body_;
}

std::optional<std::string_view> Response::header(std::string_view name) const noexcept
{
    return find_header(headers_, name);
}

void Response::add_header(std::string name, std::string value)
{
    validate_header_name(name);
    validate_header_value(value);
    headers_.push_back(Header{std::move(name), std::move(value)});
}

void Response::set_body(std::string body)
{
    body_ = std::move(body);
}

std::string Response::serialize() const
{
    std::string message = "RTSP/1.0 ";
    message += std::to_string(status_code_);
    message += ' ';
    message += reason_;
    message += "\r\nCSeq: ";
    message += std::to_string(cseq_);
    message += "\r\n";
    append_headers(message, headers_);
    append_body(message, body_);
    return message;
}

} // namespace voip::rtsp
