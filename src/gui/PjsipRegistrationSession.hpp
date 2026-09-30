#pragma once

#include "gui/QtRegistrationBackend.hpp"
#include "pjsip/SipRegistrationClient.hpp"

namespace voip::gui {

// Uses the already-verified Phase 6 registration client. Created, started,
// waited and destroyed entirely on QtRegistrationBackend's worker thread.
class PjsipRegistrationSession final : public RegistrationSession
{
  public:
    PjsipRegistrationSession(pjsip::SipEndpointConfig endpoint, pjsip::SipAccountConfig account);
    void start() override;
    [[nodiscard]] bool wait_for_registration(std::chrono::milliseconds timeout) override;
    void stop() noexcept override;

  private:
    pjsip::SipRegistrationClient client_;
};

} // namespace voip::gui
