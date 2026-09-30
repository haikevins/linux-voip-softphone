#include "gui/PjsipRegistrationSession.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace voip::gui {
namespace {

[[nodiscard]] bool is_dialable_peer(const std::string_view peer)
{
    return !peer.empty() && peer.size() <= 64U &&
           std::all_of(peer.begin(), peer.end(), [](const char value) {
               return (value >= '0' && value <= '9') || value == '*' || value == '#' ||
                      value == '+';
           });
}

} // namespace

PjsipRegistrationSession::PjsipRegistrationSession(const pjsip::SipEndpointConfig &endpoint,
                                                   const pjsip::SipAccountConfig &account)
    : server_(account.server), server_port_(account.server_port), transport_(endpoint.transport),
      client_(endpoint, account)
{}

void PjsipRegistrationSession::start()
{
    client_.start();
}

bool PjsipRegistrationSession::wait_for_registration(const std::chrono::milliseconds timeout)
{
    const auto result = client_.wait_for_registration(timeout);
    return result.registered && !result.timed_out;
}

void PjsipRegistrationSession::stop() noexcept
{
    client_.stop();
}

void PjsipRegistrationSession::start_call(const std::string_view extension)
{
    const bool valid = is_dialable_peer(extension);
    if (!valid) {
        throw std::invalid_argument("outgoing destination is not a dialable extension");
    }
    const std::string uri = "sip:" + std::string{extension} + "@" + server_ + ":" +
                            std::to_string(server_port_) +
                            ";transport=" + std::string{pjsip::to_string(transport_)};
    client_.make_call(uri);
}

IncomingCallNotice
PjsipRegistrationSession::poll_incoming_call(const std::chrono::milliseconds timeout)
{
    const auto result = client_.wait_for_incoming_call(timeout);
    if (result.timed_out) {
        return {};
    }
    if (result.state == "ERROR") {
        throw std::runtime_error("incoming call callback failed");
    }
    if (!result.received) {
        return {};
    }
    return {.received = true,
            .disconnected = result.disconnected,
            .peer = sanitize_peer(result.remote_uri)};
}

void PjsipRegistrationSession::answer_call()
{
    client_.answer_incoming_call();
}

void PjsipRegistrationSession::decline_call()
{
    client_.decline_incoming_call();
}

CallProgress PjsipRegistrationSession::poll_call(const std::chrono::milliseconds timeout)
{
    const auto result = client_.wait_for_call(timeout);
    if (result.state == "ERROR" || result.reason.starts_with("audio media error:")) {
        return CallProgress::Failed;
    }
    if (result.disconnected) {
        if (result.connected && result.media_connected) {
            return CallProgress::DisconnectedAfterConnect;
        }
        return CallProgress::Disconnected;
    }
    if (result.connected && result.media_connected) {
        return CallProgress::ConnectedWithMedia;
    }
    if (result.state == "EARLY" || result.state == "INCOMING") {
        return CallProgress::Ringing;
    }
    return CallProgress::Pending;
}

void PjsipRegistrationSession::hangup_call() noexcept
{
    client_.hangup();
}

bool PjsipRegistrationSession::wait_for_disconnection(const std::chrono::milliseconds timeout)
{
    return client_.wait_for_disconnection(timeout);
}

bool PjsipRegistrationSession::release_disconnected_call()
{
    return client_.release_disconnected_call();
}

std::string PjsipRegistrationSession::sanitize_peer(const std::string_view remote_uri)
{
    auto scheme = remote_uri.find("sip:");
    std::size_t scheme_length = 4U;
    if (scheme == std::string_view::npos) {
        scheme = remote_uri.find("sips:");
        scheme_length = 5U;
    }
    if (scheme == std::string_view::npos) {
        return "Unknown caller";
    }

    const auto user_begin = scheme + scheme_length;
    const auto user_end = remote_uri.find('@', user_begin);
    if (user_end == std::string_view::npos || user_end <= user_begin) {
        return "Unknown caller";
    }

    const auto user = remote_uri.substr(user_begin, user_end - user_begin);
    if (!is_dialable_peer(user)) {
        return "Unknown caller";
    }
    return std::string{user};
}

} // namespace voip::gui
