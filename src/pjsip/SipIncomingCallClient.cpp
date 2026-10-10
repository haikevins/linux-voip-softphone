#include "pjsip/SipIncomingCallClient.hpp"

#include "pjsip/CallbackActivityGate.hpp"
#include "pjsip/PjsipCallSlotCleanup.hpp"
#include "pjsip/Pjsua2AudioDeviceManager.hpp"
#include "pjsip/Pjsua2CallAudioBridge.hpp"

#include <atomic>
#include <cstdio>
#include <exception>
#include <pjsua-lib/pjsua.h>
#include <stdexcept>
#include <thread>
#include <utility>

namespace voip::pjsip {

struct SipIncomingCallClient::CallbackContext final
{
    explicit CallbackContext(SipIncomingCallClient &owner) noexcept : owner_(&owner) {}

    [[nodiscard]] SipIncomingCallClient *owner() const noexcept
    {
        return owner_.load(std::memory_order_acquire);
    }

    void detach_owner() noexcept
    {
        owner_.store(nullptr, std::memory_order_release);
    }

    detail::CallbackActivityGate account_activity;
    detail::CallbackActivityGate call_activity;

  private:
    std::atomic<SipIncomingCallClient *> owner_;
};

class SipIncomingCallClient::IncomingAccount final : public pj::Account
{
  public:
    explicit IncomingAccount(std::shared_ptr<CallbackContext> context)
        : context_(std::move(context))
    {}

    void onRegState(pj::OnRegStateParam &parameter) override
    {
        auto callback_entry = context_->account_activity.enter();
        if (!callback_entry.admitted()) {
            return;
        }

        auto *owner = context_->owner();
        if (owner == nullptr) {
            return;
        }

        try {
            owner->on_registration_state(parameter, getInfo());
        } catch (const pj::Error &error) {
            owner->on_registration_callback_error(error.info());
        } catch (const std::exception &error) {
            owner->on_registration_callback_error(error.what());
        } catch (...) {
            owner->on_registration_callback_error("unknown registration callback failure");
        }
    }

    void onIncomingCall(pj::OnIncomingCallParam &parameter) override
    {
        auto callback_entry = context_->account_activity.enter();
        if (!callback_entry.admitted()) {
            try {
                pj::Call rejected_call{*this, parameter.callId};
                pj::CallOpParam busy_parameter;
                busy_parameter.statusCode = PJSIP_SC_BUSY_HERE;
                rejected_call.answer(busy_parameter);
            } catch (...) {
                std::fputs("PJSIP incoming call rejection failed during shutdown\n", stderr);
            }
            return;
        }

        auto *owner = context_->owner();
        if (owner == nullptr) {
            return;
        }

        const std::lock_guard callback_lock{owner->incoming_callback_mutex_};

        try {
            owner->on_incoming_call(*this, parameter.callId);
        } catch (const pj::Error &error) {
            owner->on_incoming_callback_error(error.info());
        } catch (const std::exception &error) {
            owner->on_incoming_callback_error(error.what());
        } catch (...) {
            owner->on_incoming_callback_error("unknown incoming call callback failure");
        }
    }

  private:
    std::shared_ptr<CallbackContext> context_;
};

class SipIncomingCallClient::TrackedIncomingCall final : public pj::Call
{
  public:
    TrackedIncomingCall(pj::Account &account, std::shared_ptr<CallbackContext> context,
                        const int call_id)
        : pj::Call(account, call_id), context_(std::move(context))
    {}

    void onCallState(pj::OnCallStateParam & /*parameter*/) override
    {
        auto callback_entry = context_->call_activity.enter();
        if (!callback_entry.admitted()) {
            return;
        }

        auto *owner = context_->owner();
        if (owner == nullptr) {
            return;
        }

        const std::lock_guard callback_lock{owner->call_callback_mutex_};

        try {
            owner->on_call_state(getInfo());
        } catch (const pj::Error &error) {
            owner->on_call_callback_error(error.info());
        } catch (const std::exception &error) {
            owner->on_call_callback_error(error.what());
        } catch (...) {
            owner->on_call_callback_error("unknown incoming call state callback failure");
        }
    }

    void onCallMediaState(pj::OnCallMediaStateParam & /*parameter*/) override
    {
        auto callback_entry = context_->call_activity.enter();
        if (!callback_entry.admitted()) {
            return;
        }

        auto *owner = context_->owner();
        if (owner == nullptr) {
            return;
        }

        const std::lock_guard callback_lock{owner->call_callback_mutex_};

        try {
            owner->on_call_media_state(*this);
        } catch (const pj::Error &error) {
            owner->on_call_media_callback_error(error.info());
        } catch (const std::exception &error) {
            owner->on_call_media_callback_error(error.what());
        } catch (...) {
            owner->on_call_media_callback_error("unknown incoming call media callback failure");
        }
    }

  private:
    std::shared_ptr<CallbackContext> context_;
};

SipIncomingCallClient::SipIncomingCallClient(SipEndpointConfig endpoint_config,
                                             SipAccountConfig account_config)
    : SipIncomingCallClient(endpoint_config, std::move(account_config), {})
{}

SipIncomingCallClient::SipIncomingCallClient(SipEndpointConfig endpoint_config,
                                             SipAccountConfig account_config,
                                             SipCallAudioConfig audio_config)
    : endpoint_config_(endpoint_config), account_config_(std::move(account_config)),
      audio_config_(std::move(audio_config)),
      callback_context_(std::make_shared<CallbackContext>(*this))
{}

SipIncomingCallClient::~SipIncomingCallClient()
{
    stop();
    if (library_created_ || endpoint_ != nullptr || account_ != nullptr || call_ != nullptr) {
        std::fputs("PJSIP incoming client teardown could not prove safe destruction\n", stderr);
        std::terminate();
    }
    callback_context_->detach_owner();
}

void SipIncomingCallClient::start()
{
    if (started_) {
        throw std::logic_error("SIP incoming call client is already started");
    }
    if (library_created_ || endpoint_ != nullptr || account_ != nullptr || call_ != nullptr) {
        throw std::logic_error("SIP incoming call client retains teardown state");
    }
    if (!callback_context_->account_activity.reset_if_empty() ||
        !callback_context_->call_activity.reset_if_empty()) {
        throw std::logic_error("SIP incoming callback activity did not drain before restart");
    }
    if (!account_config_.register_on_add) {
        throw std::invalid_argument("SIP incoming call client requires register_on_add=true");
    }
    if (account_config_.transport != endpoint_config_.transport) {
        throw std::invalid_argument("SIP account and endpoint transports must match");
    }

    account_config_.validate();

    {
        const std::lock_guard lock{registration_mutex_};
        registration_result_ = {};
        registration_complete_ = false;
    }
    {
        const std::lock_guard lock{call_mutex_};
        call_result_ = {};
        incoming_complete_ = false;
        call_complete_ = false;
        call_disconnected_ = false;
    }

    try {
        endpoint_ = std::make_unique<pj::Endpoint>();
        endpoint_->libCreate();
        library_created_ = true;

        pj::EpConfig endpoint_library_config;
        endpoint_library_config.logConfig.msgLogging = false;
        endpoint_library_config.logConfig.consoleLevel = 3;
        endpoint_->libInit(endpoint_library_config);

        const auto transport_id =
            endpoint_->transportCreate(to_pjsip_transport_type(endpoint_config_.transport),
                                       endpoint_config_.to_pjsua2_transport_config());

        endpoint_->libStart();
        static_cast<void>(configure_pjsua2_audio_devices(endpoint_->audDevManager(),
                                                         audio_config_.capture_device,
                                                         audio_config_.playback_device));

        auto pjsua2_account_config = account_config_.to_pjsua2_account_config();
        pjsua2_account_config.sipConfig.transportId = transport_id;

        account_ = std::make_unique<IncomingAccount>(callback_context_);
        account_->create(pjsua2_account_config);
        started_ = true;
    } catch (...) {
        stop();
        throw;
    }
}

void SipIncomingCallClient::stop() noexcept
{
    if (!library_created_) {
        if (account_ != nullptr || call_ != nullptr) {
            std::fputs("PJSIP incoming shutdown found callback objects without a library\n",
                       stderr);
            return;
        }
        endpoint_.reset();
        started_ = false;
        return;
    }
    if (endpoint_ == nullptr) {
        std::fputs("PJSIP incoming shutdown lost the endpoint owner\n", stderr);
        return;
    }

    callback_context_->account_activity.close();
    if (!callback_context_->account_activity.wait_for_empty_for(std::chrono::seconds{2})) {
        std::fputs("PJSIP incoming account callbacks did not drain before call shutdown\n", stderr);
        return;
    }

    bool call_present{false};
    {
        const std::lock_guard call_object_lock{call_object_mutex_};
        call_present = call_ != nullptr;
    }

    if (call_present) {
        hangup();
        if (!wait_for_disconnection(std::chrono::seconds{2})) {
            std::fputs("PJSIP incoming call did not reach DISCONNECTED during shutdown\n", stderr);
            return;
        }
    }

    callback_context_->call_activity.close();
    if (!callback_context_->call_activity.wait_for_empty_for(std::chrono::seconds{2})) {
        std::fputs("PJSIP incoming call callbacks did not drain during shutdown\n", stderr);
        return;
    }

    if (!wait_for_call_cleanup(std::chrono::seconds{2})) {
        return;
    }

    {
        const std::lock_guard incoming_callback_lock{incoming_callback_mutex_};
        const std::lock_guard call_callback_lock{call_callback_mutex_};
        const std::lock_guard call_object_lock{call_object_mutex_};
        call_.reset();
    }

    if (!shutdown_account_safely()) {
        return;
    }
    if (!callback_context_->account_activity.wait_for_empty_for(std::chrono::seconds{2})) {
        std::fputs("PJSIP incoming account callbacks did not drain after account shutdown\n",
                   stderr);
        return;
    }
    // Keep the derived Account callback target alive through libDestroy().
    // PJProject 2.17 joins UA/media workers and destroys the SIP endpoint
    // inside this boundary. This implementation-reviewed ordering is not a
    // universal proof of external callback quiescence.
    try {
        endpoint_->libDestroy();
    } catch (...) {
        std::fputs("PJSIP cleanup failed during incoming call shutdown\n", stderr);
        return;
    }

    account_.reset();
    endpoint_.reset();
    library_created_ = false;
    started_ = false;
}

RegistrationResult
SipIncomingCallClient::wait_for_registration(const std::chrono::milliseconds timeout)
{
    if (!started_) {
        throw std::logic_error("SIP incoming call client is not started");
    }
    if (timeout <= std::chrono::milliseconds::zero()) {
        throw std::invalid_argument("registration timeout must be positive");
    }

    std::unique_lock lock{registration_mutex_};
    const auto completed =
        registration_changed_.wait_for(lock, timeout, [this] { return registration_complete_; });

    if (!completed) {
        auto result = registration_result_;
        result.registered = false;
        result.timed_out = true;
        result.reason = "registration timed out";
        return result;
    }

    return registration_result_;
}

IncomingCallResult
SipIncomingCallClient::wait_for_incoming_call(const std::chrono::milliseconds timeout)
{
    if (!started_) {
        throw std::logic_error("SIP incoming call client is not started");
    }
    if (timeout <= std::chrono::milliseconds::zero()) {
        throw std::invalid_argument("incoming call timeout must be positive");
    }

    std::unique_lock lock{call_mutex_};
    const auto completed =
        call_changed_.wait_for(lock, timeout, [this] { return incoming_complete_; });

    if (!completed) {
        auto result = call_result_;
        result.timed_out = true;
        result.reason = "incoming call timed out";
        return result;
    }

    return call_result_;
}

void SipIncomingCallClient::answer()
{
    {
        const std::lock_guard state_lock{call_mutex_};
        if (!call_result_.received) {
            throw std::logic_error("no incoming call is waiting to be answered");
        }
        if (call_disconnected_) {
            throw std::logic_error("incoming call disconnected before it could be answered");
        }
    }

    const std::lock_guard call_object_lock{call_object_mutex_};
    if (call_ == nullptr) {
        throw std::logic_error("incoming call object is not available");
    }

    pj::CallOpParam parameter;
    parameter.statusCode = PJSIP_SC_OK;
    call_->answer(parameter);
}

IncomingCallResult SipIncomingCallClient::wait_for_call(const std::chrono::milliseconds timeout)
{
    {
        const std::lock_guard state_lock{call_mutex_};
        if (!call_result_.received) {
            throw std::logic_error("no incoming call has been received");
        }
    }
    if (timeout <= std::chrono::milliseconds::zero()) {
        throw std::invalid_argument("call timeout must be positive");
    }

    std::unique_lock lock{call_mutex_};
    const auto completed = call_changed_.wait_for(lock, timeout, [this] { return call_complete_; });

    if (!completed) {
        auto result = call_result_;
        result.timed_out = true;
        result.reason = "call confirmation timed out";
        return result;
    }

    return call_result_;
}

std::vector<RtpMediaDiagnostics> SipIncomingCallClient::rtp_diagnostics()
{
    if (!started_) {
        throw std::logic_error("SIP incoming call client is not started");
    }

    const std::lock_guard callback_lock{call_callback_mutex_};
    const std::lock_guard call_object_lock{call_object_mutex_};
    if (call_ == nullptr) {
        throw std::logic_error("no incoming call is available for RTP diagnostics");
    }

    return read_active_audio_rtp_diagnostics(*call_);
}

void SipIncomingCallClient::hangup() noexcept
{
    {
        const std::lock_guard state_lock{call_mutex_};
        if (call_disconnected_) {
            return;
        }
    }

    const std::lock_guard call_object_lock{call_object_mutex_};
    if (call_ == nullptr) {
        return;
    }

    try {
        const pj::CallOpParam parameter;
        call_->hangup(parameter);
    } catch (...) {
        std::fputs("PJSIP incoming call hangup failed during shutdown\n", stderr);
    }
}

bool SipIncomingCallClient::wait_for_disconnection(const std::chrono::milliseconds timeout)
{
    if (timeout <= std::chrono::milliseconds::zero()) {
        throw std::invalid_argument("disconnection timeout must be positive");
    }

    std::unique_lock lock{call_mutex_};
    return call_changed_.wait_for(lock, timeout, [this] { return call_disconnected_; });
}

bool SipIncomingCallClient::wait_for_call_cleanup(const std::chrono::milliseconds timeout) noexcept
{
    const bool slots_drained = detail::wait_for_zero_call_slots(
        timeout, [] { return pjsua_call_get_count(); },
        [] { return std::chrono::steady_clock::now(); },
        [](const std::chrono::milliseconds delay) { std::this_thread::sleep_for(delay); });
    if (!slots_drained) {
        std::fputs("PJSIP call slot remained busy during incoming call shutdown\n", stderr);
    }
    return slots_drained;
}

bool SipIncomingCallClient::shutdown_account_safely() noexcept
{
    if (account_ == nullptr || !account_->isValid()) {
        return true;
    }

    pj::AccountShutdownParam parameter;
    parameter.force = false;

    try {
        account_->shutdown2(parameter);
        return true;
    } catch (const pj::Error &error) {
        std::fprintf(stderr, "PJSIP account shutdown failed: %s\n", error.info().c_str());
        return false;
    } catch (...) {
        std::fputs("PJSIP account shutdown failed during incoming call cleanup\n", stderr);
        return false;
    }
}

void SipIncomingCallClient::on_registration_state(const pj::OnRegStateParam &parameter,
                                                  const pj::AccountInfo &info)
{
    {
        const std::lock_guard lock{registration_mutex_};
        registration_result_.registered = info.regIsActive;
        registration_result_.timed_out = false;
        registration_result_.sip_status_code = static_cast<int>(parameter.code);
        registration_result_.reason = parameter.reason;

        const auto status_code = static_cast<int>(parameter.code);
        registration_complete_ =
            info.regIsActive || parameter.status != PJ_SUCCESS || status_code >= 200;
    }

    registration_changed_.notify_all();
}

void SipIncomingCallClient::on_registration_callback_error(std::string reason) noexcept
{
    try {
        {
            const std::lock_guard lock{registration_mutex_};
            registration_result_.registered = false;
            registration_result_.timed_out = false;
            registration_result_.sip_status_code = 0;
            registration_result_.reason = std::move(reason);
            registration_complete_ = true;
        }
        registration_changed_.notify_all();
    } catch (...) {
        std::fputs("PJSIP incoming registration callback error handling failed\n", stderr);
        registration_changed_.notify_all();
    }
}

void SipIncomingCallClient::on_incoming_call(pj::Account &account, const int call_id)
{
    std::string remote_uri;
    std::string state;
    int status_code{0};
    std::string reason;

    {
        const std::lock_guard call_object_lock{call_object_mutex_};

        if (call_ != nullptr) {
            pj::Call rejected_call{account, call_id};
            pj::CallOpParam busy_parameter;
            busy_parameter.statusCode = PJSIP_SC_BUSY_HERE;
            rejected_call.answer(busy_parameter);
            return;
        }

        call_ = std::make_unique<TrackedIncomingCall>(account, callback_context_, call_id);
        const auto info = call_->getInfo();
        remote_uri = info.remoteUri;
        state = info.stateText;
        status_code = static_cast<int>(info.lastStatusCode);
        reason = info.lastReason;

        pj::CallOpParam ringing_parameter;
        ringing_parameter.statusCode = PJSIP_SC_RINGING;
        call_->answer(ringing_parameter);
    }

    {
        const std::lock_guard lock{call_mutex_};
        call_result_.received = true;
        call_result_.timed_out = false;
        if (call_result_.sip_status_code == 0) {
            call_result_.sip_status_code = status_code;
        }
        if (call_result_.state.empty()) {
            call_result_.state = std::move(state);
        }
        if (call_result_.reason.empty()) {
            call_result_.reason = std::move(reason);
        }
        if (call_result_.remote_uri.empty()) {
            call_result_.remote_uri = std::move(remote_uri);
        }
        incoming_complete_ = true;
    }

    call_changed_.notify_all();
}

void SipIncomingCallClient::on_incoming_callback_error(std::string reason) noexcept
{
    try {
        {
            const std::lock_guard lock{call_mutex_};
            call_result_.received = false;
            call_result_.connected = false;
            call_result_.disconnected = false;
            call_result_.timed_out = false;
            call_result_.sip_status_code = 0;
            call_result_.state = "ERROR";
            call_result_.reason = std::move(reason);
            incoming_complete_ = true;
            call_complete_ = true;
        }
        call_changed_.notify_all();
    } catch (...) {
        std::fputs("PJSIP incoming callback error handling failed\n", stderr);
        call_changed_.notify_all();
    }
}

void SipIncomingCallClient::on_call_state(const pj::CallInfo &info)
{
    {
        const std::lock_guard lock{call_mutex_};
        const auto connected = info.state == PJSIP_INV_STATE_CONFIRMED;
        const auto disconnected = info.state == PJSIP_INV_STATE_DISCONNECTED;

        call_result_.connected = call_result_.connected || connected;
        call_result_.disconnected = disconnected;
        call_result_.timed_out = false;
        call_result_.sip_status_code = static_cast<int>(info.lastStatusCode);
        call_result_.state = info.stateText;
        call_result_.reason = info.lastReason;
        call_result_.remote_uri = info.remoteUri;

        call_disconnected_ = disconnected;
        call_complete_ = disconnected || (call_result_.connected && call_result_.media_connected);
    }

    call_changed_.notify_all();
}

void SipIncomingCallClient::on_call_media_state(pj::Call &call)
{
    const auto call_info = call.getInfo();
    if (active_audio_media_indices(call_info).empty()) {
        return;
    }

    if (endpoint_ == nullptr) {
        throw std::logic_error("PJSIP endpoint is not available for incoming call audio");
    }

    connect_active_call_audio(call, endpoint_->audDevManager());

    {
        const std::lock_guard lock{call_mutex_};
        call_result_.media_connected = true;
        call_complete_ =
            call_disconnected_ || (call_result_.connected && call_result_.media_connected);
    }

    call_changed_.notify_all();
}

void SipIncomingCallClient::on_call_callback_error(std::string reason) noexcept
{
    try {
        {
            const std::lock_guard lock{call_mutex_};
            call_result_.connected = false;
            call_result_.disconnected = false;
            call_result_.timed_out = false;
            call_result_.sip_status_code = 0;
            call_result_.state = "ERROR";
            call_result_.reason = std::move(reason);
            call_complete_ = true;
        }
        call_changed_.notify_all();
    } catch (...) {
        std::fputs("PJSIP incoming call state callback error handling failed\n", stderr);
        call_changed_.notify_all();
    }
}

void SipIncomingCallClient::on_call_media_callback_error(std::string reason) noexcept
{
    try {
        {
            const std::lock_guard lock{call_mutex_};
            call_result_.media_connected = false;
            call_result_.timed_out = false;
            call_result_.reason = "audio media error: " + std::move(reason);
            call_complete_ = true;
        }
        call_changed_.notify_all();
    } catch (...) {
        std::fputs("PJSIP incoming call media callback error handling failed\n", stderr);
        call_changed_.notify_all();
    }
}

} // namespace voip::pjsip
