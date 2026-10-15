#pragma once

#include "application/SipEngine.hpp"
#include "pjsip/SipAccountConfig.hpp"
#include "pjsip/SipCallAudioConfig.hpp"
#include "pjsip/SipEndpointConfig.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>

namespace voip::pjsip {

class Pjsua2SipEngine final : public application::SipEngine
{
  public:
    Pjsua2SipEngine(SipEndpointConfig endpoint_config, SipAccountConfig account_config);
    Pjsua2SipEngine(SipEndpointConfig endpoint_config, SipAccountConfig account_config,
                    SipCallAudioConfig audio_config);
    ~Pjsua2SipEngine() override;

    Pjsua2SipEngine(const Pjsua2SipEngine &) = delete;
    Pjsua2SipEngine &operator=(const Pjsua2SipEngine &) = delete;
    Pjsua2SipEngine(Pjsua2SipEngine &&) = delete;
    Pjsua2SipEngine &operator=(Pjsua2SipEngine &&) = delete;

    void set_observer(application::SipEngineObserver *observer) noexcept override;

    void register_account() override;
    void make_call(const std::string &destination) override;
    void answer_call() override;
    void hangup_call() override;

  private:
    class EngineAccount;
    class EngineCall;

    void stop() noexcept;
    [[nodiscard]] bool shutdown_account_safely() noexcept;
    [[nodiscard]] bool wait_for_call_cleanup() noexcept;
    [[nodiscard]] std::string destination_uri(const std::string &destination) const;

    void on_registration_state(const pj::OnRegStateParam &parameter, const pj::AccountInfo &info);
    void on_registration_callback_error(std::string reason) noexcept;
    void on_incoming_call(pj::Account &account, int call_id);
    void on_incoming_callback_error(std::string reason) noexcept;
    void on_call_state(const pj::CallInfo &info, bool incoming);
    void on_call_media_state(pj::Call &call);
    void on_call_callback_error(std::string reason) noexcept;

    void notify_registration_succeeded() noexcept;
    void notify_registration_failed(std::string reason) noexcept;
    void notify_incoming_call(std::string peer) noexcept;
    void notify_remote_ringing() noexcept;
    void notify_call_connected() noexcept;
    void notify_call_ended() noexcept;
    void notify_call_failed(std::string reason) noexcept;

    SipEndpointConfig endpoint_config_;
    SipAccountConfig account_config_;
    SipCallAudioConfig audio_config_;

    std::unique_ptr<pj::Endpoint> endpoint_;
    std::unique_ptr<EngineAccount> account_;
    std::unique_ptr<EngineCall> call_;

    std::mutex observer_mutex_;
    application::SipEngineObserver *observer_{nullptr};

    std::mutex incoming_callback_mutex_;
    std::mutex call_callback_mutex_;
    std::mutex call_object_mutex_;

    std::atomic_bool registered_{false};
    std::atomic_bool call_active_{false};
    std::atomic_bool call_connected_{false};
    std::atomic_bool incoming_call_{false};
    std::atomic_bool hangup_requested_{false};
    std::atomic_bool suppress_callbacks_{false};

    bool library_created_{false};
    bool started_{false};
};

} // namespace voip::pjsip
