#include "gui/PjsipRegistrationSession.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace voip::gui {

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
    // Defense in depth: reject any URI/control-character injection even if a
    // caller accidentally bypasses CallManager's dialable-destination check.
    const bool valid =
        !extension.empty() && extension.size() <= 64U &&
        std::all_of(extension.begin(), extension.end(), [](const char digit) {
            return (digit >= '0' && digit <= '9') || digit == '*' || digit == '#' || digit == '+';
        });
    if (!valid) {
        throw std::invalid_argument("outgoing destination is not a dialable extension");
    }
    // Never accept a URI/hostname through QML; use the configured SIP server.
    const std::string uri = "sip:" + std::string{extension} + "@" + server_ + ":" +
                            std::to_string(server_port_) +
                            ";transport=" + std::string{pjsip::to_string(transport_)};
    client_.make_call(uri);
}

CallProgress PjsipRegistrationSession::poll_call(const std::chrono::milliseconds timeout)
{
    const auto result = client_.wait_for_call(timeout);
    if (result.state == "ERROR" || result.reason.starts_with("audio media error:")) {
        return CallProgress::Failed;
    }
    if (result.disconnected) {
        // A remote BYE can race the worker before it has published the
        // ConnectedWithMedia state. Preserve the cumulative PJSIP history so
        // a call that really reached connected media is treated as a normal
        // remote hangup, not as a failed call attempt.
        if (result.connected && result.media_connected) {
            return CallProgress::DisconnectedAfterConnect;
        }
        return CallProgress::Disconnected;
    }
    if (result.connected && result.media_connected) {
        return CallProgress::ConnectedWithMedia;
    }
    if (result.state == "EARLY") {
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

} // namespace voip::gui
