#pragma once

#include "network/Ipv4Endpoint.hpp"
#include "rtsp/RtspConnection.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

namespace voip::rtsp {

enum class ClientState
{
    Disconnected,
    Connected,
    Described,
    Ready,
    Playing,
};

struct ClientConfig
{
    network::Ipv4Endpoint server_endpoint;
    std::string base_uri;
    std::uint16_t client_rtp_port{5000U};
    std::uint16_t client_rtcp_port{5001U};
};

class SessionError : public std::runtime_error
{
  public:
    using std::runtime_error::runtime_error;
};

class ClientSession
{
  public:
    explicit ClientSession(ClientConfig config);
    ClientSession(ClientConfig config, std::unique_ptr<Connection> connection);

    ClientSession(const ClientSession &) = delete;
    ClientSession &operator=(const ClientSession &) = delete;
    ClientSession(ClientSession &&) = delete;
    ClientSession &operator=(ClientSession &&) = delete;

    void connect();
    void disconnect() noexcept;

    [[nodiscard]] Response options();
    [[nodiscard]] Response describe();
    [[nodiscard]] Response setup(std::string control_uri);
    [[nodiscard]] Response play();
    [[nodiscard]] Response teardown();

    [[nodiscard]] ClientState state() const noexcept;
    [[nodiscard]] std::optional<std::string> session_id() const;

  private:
    [[nodiscard]] Request make_request(Method method, std::string uri);
    [[nodiscard]] Response exchange(const Request &request);
    void validate_config() const;

    std::unique_ptr<Connection> connection_;
    ClientConfig config_;
    ClientState state_{ClientState::Disconnected};
    int next_cseq_{1};
    std::optional<std::string> session_id_;
};

} // namespace voip::rtsp
