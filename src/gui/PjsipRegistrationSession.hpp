#pragma once

#include "gui/QtRegistrationBackend.hpp"
#include "pjsip/SipOutgoingCallClient.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace voip::gui {

// Reuses one Phase 6 PJSUA2 endpoint/account for registration plus both call
// directions. All PJSUA2 operations stay on QtRegistrationBackend's worker.
class PjsipRegistrationSession final : public RegistrationSession
{
  public:
    PjsipRegistrationSession(const pjsip::SipEndpointConfig &endpoint,
                             const pjsip::SipAccountConfig &account);
    void start() override;
    [[nodiscard]] bool wait_for_registration(std::chrono::milliseconds timeout) override;
    void stop() noexcept override;
    [[nodiscard]] bool supports_calls() const noexcept override
    {
        return true;
    }
    [[nodiscard]] bool supports_incoming_calls() const noexcept override
    {
        return true;
    }
    void start_call(std::string_view extension) override;
    [[nodiscard]] IncomingCallNotice poll_incoming_call(std::chrono::milliseconds timeout) override;
    void answer_call() override;
    void decline_call() override;
    [[nodiscard]] RtpDiagnosticsSnapshot read_rtp_diagnostics() override;
    [[nodiscard]] CallProgress poll_call(std::chrono::milliseconds timeout) override;
    void hangup_call() noexcept override;
    [[nodiscard]] bool wait_for_disconnection(std::chrono::milliseconds timeout) override;
    [[nodiscard]] bool release_disconnected_call() override;

  private:
    [[nodiscard]] static std::string sanitize_peer(std::string_view remote_uri);
    [[nodiscard]] static std::string sanitize_codec(std::string_view codec);

    const std::string server_;
    const std::uint16_t server_port_;
    const pjsip::SipTransport transport_;
    pjsip::SipOutgoingCallClient client_;
};

} // namespace voip::gui
