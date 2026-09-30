#pragma once

#include "gui/QtRegistrationBackend.hpp"
#include "pjsip/SipOutgoingCallClient.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace voip::gui {

// Reuses the Phase 6 outgoing client's account, audio bridge and RTP pipeline.
// All operations, including hangup/cleanup, run on QtRegistrationBackend worker.
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
    void start_call(std::string_view extension) override;
    [[nodiscard]] CallProgress poll_call(std::chrono::milliseconds timeout) override;
    void hangup_call() noexcept override;
    [[nodiscard]] bool wait_for_disconnection(std::chrono::milliseconds timeout) override;
    [[nodiscard]] bool release_disconnected_call() override;

  private:
    const std::string server_;
    const std::uint16_t server_port_;
    const pjsip::SipTransport transport_;
    pjsip::SipOutgoingCallClient client_;
};

} // namespace voip::gui
