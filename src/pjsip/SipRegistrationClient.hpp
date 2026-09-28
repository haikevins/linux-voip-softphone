#pragma once

#include "pjsip/SipAccountConfig.hpp"
#include "pjsip/SipEndpointConfig.hpp"

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>

namespace voip::pjsip {

struct RegistrationResult
{
    bool registered{false};
    bool timed_out{false};
    int sip_status_code{0};
    std::string reason;
};

class SipRegistrationClient
{
  public:
    SipRegistrationClient(SipEndpointConfig endpoint_config, SipAccountConfig account_config);
    ~SipRegistrationClient();

    SipRegistrationClient(const SipRegistrationClient &) = delete;
    SipRegistrationClient &operator=(const SipRegistrationClient &) = delete;
    SipRegistrationClient(SipRegistrationClient &&) = delete;
    SipRegistrationClient &operator=(SipRegistrationClient &&) = delete;

    void start();
    void stop() noexcept;

    [[nodiscard]] RegistrationResult wait_for_registration(std::chrono::milliseconds timeout);

  private:
    class RegistrationAccount;

    void on_registration_state(const pj::OnRegStateParam &parameter, const pj::AccountInfo &info);
    void on_registration_callback_error(std::string reason) noexcept;

    SipEndpointConfig endpoint_config_;
    SipAccountConfig account_config_;
    std::unique_ptr<pj::Endpoint> endpoint_;
    std::unique_ptr<RegistrationAccount> account_;

    std::mutex registration_mutex_;
    std::condition_variable registration_changed_;
    RegistrationResult registration_result_;
    bool registration_complete_{false};
    bool library_created_{false};
    bool started_{false};
};

} // namespace voip::pjsip
