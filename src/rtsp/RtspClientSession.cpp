#include "rtsp/RtspClientSession.hpp"

#include "rtsp/RtspTcpConnection.hpp"

#include <string_view>
#include <utility>

namespace voip::rtsp {
namespace {

std::string parse_session_id(std::string_view value)
{
    const auto parameter = value.find(';');
    const auto id = value.substr(0U, parameter);
    if (id.empty()) {
        throw SessionError("RTSP SETUP response contains an empty Session header");
    }
    return std::string{id};
}

std::string transport_header(const ClientConfig &config)
{
    return "RTP/AVP;unicast;client_port=" + std::to_string(config.client_rtp_port) + '-' +
           std::to_string(config.client_rtcp_port);
}

} // namespace

ClientSession::ClientSession(ClientConfig config)
    : connection_(std::make_unique<TcpConnection>(config.server_endpoint)),
      config_(std::move(config))
{
    validate_config();
}

ClientSession::ClientSession(ClientConfig config, std::unique_ptr<Connection> connection)
    : connection_(std::move(connection)), config_(std::move(config))
{
    if (!connection_) {
        throw std::invalid_argument("RTSP connection must not be null");
    }
    validate_config();
}

void ClientSession::connect()
{
    if (state_ != ClientState::Disconnected) {
        return;
    }

    connection_->connect();
    state_ = ClientState::Connected;
}

void ClientSession::disconnect() noexcept
{
    connection_->disconnect();
    session_id_.reset();
    state_ = ClientState::Disconnected;
}

Response ClientSession::options()
{
    if (state_ == ClientState::Disconnected) {
        throw std::logic_error("RTSP OPTIONS requires a connected session");
    }

    return exchange(make_request(Method::Options, config_.base_uri));
}

Response ClientSession::describe()
{
    if (state_ != ClientState::Connected) {
        throw std::logic_error("RTSP DESCRIBE requires the connected state");
    }

    auto request = make_request(Method::Describe, config_.base_uri);
    request.add_header("Accept", "application/sdp");
    auto response = exchange(request);
    state_ = ClientState::Described;
    return response;
}

Response ClientSession::setup(std::string control_uri)
{
    if (state_ != ClientState::Described) {
        throw std::logic_error("RTSP SETUP requires a successful DESCRIBE first");
    }
    if (control_uri.empty()) {
        throw std::invalid_argument("RTSP control URI must not be empty");
    }

    auto request = make_request(Method::Setup, std::move(control_uri));
    request.add_header("Transport", transport_header(config_));
    auto response = exchange(request);

    const auto session = response.header("Session");
    if (!session.has_value()) {
        throw SessionError("RTSP SETUP response is missing the Session header");
    }

    session_id_ = parse_session_id(session.value_or(""));
    state_ = ClientState::Ready;
    return response;
}

Response ClientSession::play()
{
    if (state_ != ClientState::Ready || !session_id_.has_value()) {
        throw std::logic_error("RTSP PLAY requires a configured session");
    }

    auto request = make_request(Method::Play, config_.base_uri);
    request.add_header("Session", session_id_.value_or(""));
    auto response = exchange(request);
    state_ = ClientState::Playing;
    return response;
}

Response ClientSession::teardown()
{
    if ((state_ != ClientState::Ready && state_ != ClientState::Playing) ||
        !session_id_.has_value()) {
        throw std::logic_error("RTSP TEARDOWN requires an established session");
    }

    auto request = make_request(Method::Teardown, config_.base_uri);
    request.add_header("Session", session_id_.value_or(""));
    auto response = exchange(request);
    session_id_.reset();
    state_ = ClientState::Connected;
    return response;
}

ClientState ClientSession::state() const noexcept
{
    return state_;
}

std::optional<std::string> ClientSession::session_id() const
{
    return session_id_;
}

Request ClientSession::make_request(const Method method, std::string uri)
{
    return Request{method, std::move(uri), next_cseq_++};
}

Response ClientSession::exchange(const Request &request)
{
    auto response = connection_->transact(request);
    if (response.cseq() != request.cseq()) {
        throw SessionError("RTSP response CSeq does not match the request");
    }
    if (response.status_code() < 200 || response.status_code() >= 300) {
        throw SessionError("RTSP request failed with status " +
                           std::to_string(response.status_code()) + ' ' + response.reason());
    }
    return response;
}

void ClientSession::validate_config() const
{
    if (!config_.base_uri.starts_with("rtsp://")) {
        throw std::invalid_argument("RTSP base URI must start with rtsp://");
    }
    if (config_.client_rtp_port == 0U || config_.client_rtcp_port == 0U) {
        throw std::invalid_argument("RTSP client RTP/RTCP ports must be non-zero");
    }
    if (config_.client_rtp_port == config_.client_rtcp_port) {
        throw std::invalid_argument("RTSP client RTP and RTCP ports must differ");
    }
}

} // namespace voip::rtsp
