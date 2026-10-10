#pragma once

#include "pjsip/CallbackActivityGate.hpp"
#include "pjsip/Pjsua2RtpDiagnostics.hpp"
#include "pjsip/SipAccountConfig.hpp"
#include "pjsip/SipCallAudioConfig.hpp"
#include "pjsip/SipEndpointConfig.hpp"
#include "pjsip/SipRegistrationClient.hpp"

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace voip::pjsip {

struct IncomingCallResult
{
    bool received{false};
    bool connected{false};
    bool disconnected{false};
    bool media_connected{false};
    bool timed_out{false};
    int sip_status_code{0};
    std::string state;
    std::string reason;
    std::string remote_uri;
};

class SipIncomingCallClient
{
  public:
    SipIncomingCallClient(SipEndpointConfig endpoint_config, SipAccountConfig account_config);
    SipIncomingCallClient(SipEndpointConfig endpoint_config, SipAccountConfig account_config,
                          SipCallAudioConfig audio_config);
    ~SipIncomingCallClient();

    SipIncomingCallClient(const SipIncomingCallClient &) = delete;
    SipIncomingCallClient &operator=(const SipIncomingCallClient &) = delete;
    SipIncomingCallClient(SipIncomingCallClient &&) = delete;
    SipIncomingCallClient &operator=(SipIncomingCallClient &&) = delete;

    void start();
    void stop() noexcept;

    [[nodiscard]] RegistrationResult wait_for_registration(std::chrono::milliseconds timeout);
    [[nodiscard]] IncomingCallResult wait_for_incoming_call(std::chrono::milliseconds timeout);
    void answer();
    [[nodiscard]] IncomingCallResult wait_for_call(std::chrono::milliseconds timeout);
    [[nodiscard]] std::vector<RtpMediaDiagnostics> rtp_diagnostics();
    void hangup() noexcept;
    [[nodiscard]] bool wait_for_disconnection(std::chrono::milliseconds timeout);

  private:
    class IncomingAccount;
    class TrackedIncomingCall;

    void on_registration_state(const pj::OnRegStateParam &parameter, const pj::AccountInfo &info);
    void on_registration_callback_error(std::string reason) noexcept;
    void on_incoming_call(pj::Account &account, int call_id);
    void on_incoming_callback_error(std::string reason) noexcept;
    void on_call_state(const pj::CallInfo &info);
    void on_call_media_state(pj::Call &call);
    void on_call_callback_error(std::string reason) noexcept;
    void on_call_media_callback_error(std::string reason) noexcept;
    [[nodiscard]] bool wait_for_call_cleanup(std::chrono::milliseconds timeout) noexcept;
    [[nodiscard]] bool shutdown_account_safely() noexcept;

    SipEndpointConfig endpoint_config_;
    SipAccountConfig account_config_;
    SipCallAudioConfig audio_config_;
    std::unique_ptr<pj::Endpoint> endpoint_;
    std::unique_ptr<IncomingAccount> account_;
    std::unique_ptr<TrackedIncomingCall> call_;

    std::mutex registration_mutex_;
    std::condition_variable registration_changed_;
    RegistrationResult registration_result_;
    bool registration_complete_{false};

    std::mutex call_mutex_;
    std::mutex call_object_mutex_;
    std::mutex incoming_callback_mutex_;
    std::mutex call_callback_mutex_;
    std::condition_variable call_changed_;
    IncomingCallResult call_result_;
    bool incoming_complete_{false};
    bool call_complete_{false};
    bool call_disconnected_{false};

    detail::CallbackActivityGate account_callback_activity_;
    detail::CallbackActivityGate call_callback_activity_;

    bool library_created_{false};
    bool started_{false};
};

} // namespace voip::pjsip
