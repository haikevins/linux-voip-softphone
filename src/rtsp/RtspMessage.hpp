#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace voip::rtsp {

enum class Method
{
    Options,
    Describe,
    Setup,
    Play,
    Teardown,
};

[[nodiscard]] std::string_view to_string(Method method) noexcept;
[[nodiscard]] std::optional<Method> parse_method(std::string_view token) noexcept;

struct Header
{
    std::string name;
    std::string value;
};

class Request
{
  public:
    Request(Method method, std::string uri, int cseq);

    [[nodiscard]] Method method() const noexcept;
    [[nodiscard]] const std::string &uri() const noexcept;
    [[nodiscard]] int cseq() const noexcept;
    [[nodiscard]] const std::vector<Header> &headers() const noexcept;
    [[nodiscard]] const std::string &body() const noexcept;
    [[nodiscard]] std::optional<std::string_view> header(std::string_view name) const noexcept;

    void add_header(std::string name, std::string value);
    void set_body(std::string body);

    [[nodiscard]] std::string serialize() const;

  private:
    Method method_;
    std::string uri_;
    int cseq_;
    std::vector<Header> headers_;
    std::string body_;
};

class Response
{
  public:
    Response(int status_code, std::string reason, int cseq);

    [[nodiscard]] int status_code() const noexcept;
    [[nodiscard]] const std::string &reason() const noexcept;
    [[nodiscard]] int cseq() const noexcept;
    [[nodiscard]] const std::vector<Header> &headers() const noexcept;
    [[nodiscard]] const std::string &body() const noexcept;
    [[nodiscard]] std::optional<std::string_view> header(std::string_view name) const noexcept;

    void add_header(std::string name, std::string value);
    void set_body(std::string body);

    [[nodiscard]] std::string serialize() const;

  private:
    int status_code_;
    std::string reason_;
    int cseq_;
    std::vector<Header> headers_;
    std::string body_;
};

} // namespace voip::rtsp
