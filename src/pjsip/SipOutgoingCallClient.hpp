#pragma once

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

struct OutgoingCallResult
{
    bool connected{false};
    bool disconnected{false};
    bool media_connected{false};
    bool timed_out{false};
    int sip_status_code{0};
    std::string state;
    std::string reason;
    std::string remote_uri;
};

class SipOutgoingCallClient
{
  public:
    SipOutgoingCallClient(SipEndpointConfig endpoint_config, SipAccountConfig account_config);
    SipOutgoingCallClient(SipEndpointConfig endpoint_config, SipAccountConfig account_config,
                          SipCallAudioConfig audio_config);
    ~SipOutgoingCallClient();

    SipOutgoingCallClient(const SipOutgoingCallClient &) = delete;
    SipOutgoingCallClient &operator=(const SipOutgoingCallClient &) = delete;
    SipOutgoingCallClient(SipOutgoingCallClient &&) = delete;
    SipOutgoingCallClient &operator=(SipOutgoingCallClient &&) = delete;

    void start();
    void stop() noexcept;

    [[nodiscard]] RegistrationResult wait_for_registration(std::chrono::milliseconds timeout);

    void make_call(const std::string &destination_uri);
    [[nodiscard]] OutgoingCallResult wait_for_call(std::chrono::milliseconds timeout);
    [[nodiscard]] std::vector<RtpMediaDiagnostics> rtp_diagnostics();
    void hangup() noexcept;
    [[nodiscard]] bool wait_for_disconnection(std::chrono::milliseconds timeout);
    // Called on the owner worker only, after remote/local disconnect.
    [[nodiscard]] bool release_disconnected_call();

  private:
    class CallAccount;
    class TrackedCall;

    void on_registration_state(const pj::OnRegStateParam &parameter, const pj::AccountInfo &info);
    void on_registration_callback_error(std::string reason) noexcept;
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
    std::unique_ptr<CallAccount> account_;
    std::unique_ptr<TrackedCall> call_;

    std::mutex registration_mutex_;
    std::condition_variable registration_changed_;
    RegistrationResult registration_result_;
    bool registration_complete_{false};

    std::mutex call_mutex_;
    std::mutex call_callback_mutex_;
    std::condition_variable call_changed_;
    OutgoingCallResult call_result_;
    bool call_complete_{false};
    bool call_disconnected_{false};

    bool library_created_{false};
    bool started_{false};
};

} // namespace voip::pjsip
