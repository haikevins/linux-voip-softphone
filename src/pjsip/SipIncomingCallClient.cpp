#include "pjsip/SipIncomingCallClient.hpp"

#include "pjsip/Pjsua2AudioDeviceManager.hpp"
#include "pjsip/Pjsua2CallAudioBridge.hpp"

#include <cstdio>
#include <pjsua-lib/pjsua.h>
#include <stdexcept>
#include <thread>
#include <utility>

namespace voip::pjsip {

class SipIncomingCallClient::IncomingAccount final : public pj::Account
{
  public:
    explicit IncomingAccount(SipIncomingCallClient &owner) : owner_(owner) {}

    void onRegState(pj::OnRegStateParam &parameter) override
    {
        try {
            owner_.on_registration_state(parameter, getInfo());
        } catch (const pj::Error &error) {
            owner_.on_registration_callback_error(error.info());
        } catch (const std::exception &error) {
            owner_.on_registration_callback_error(error.what());
        } catch (...) {
            owner_.on_registration_callback_error("unknown registration callback failure");
        }
    }

    void onIncomingCall(pj::OnIncomingCallParam &parameter) override
    {
        const std::lock_guard callback_lock{owner_.incoming_callback_mutex_};

        try {
            owner_.on_incoming_call(*this, parameter.callId);
        } catch (const pj::Error &error) {
            owner_.on_incoming_callback_error(error.info());
        } catch (const std::exception &error) {
            owner_.on_incoming_callback_error(error.what());
        } catch (...) {
            owner_.on_incoming_callback_error("unknown incoming call callback failure");
        }
    }

  private:
    SipIncomingCallClient &owner_;
};

class SipIncomingCallClient::TrackedIncomingCall final : public pj::Call
{
  public:
    TrackedIncomingCall(pj::Account &account, SipIncomingCallClient &owner, const int call_id)
        : pj::Call(account, call_id), owner_(owner)
    {}

    void onCallState(pj::OnCallStateParam & /*parameter*/) override
    {
        const std::lock_guard callback_lock{owner_.call_callback_mutex_};

        try {
            owner_.on_call_state(getInfo());
        } catch (const pj::Error &error) {
            owner_.on_call_callback_error(error.info());
        } catch (const std::exception &error) {
            owner_.on_call_callback_error(error.what());
        } catch (...) {
            owner_.on_call_callback_error("unknown incoming call state callback failure");
        }
    }

    void onCallMediaState(pj::OnCallMediaStateParam & /*parameter*/) override
    {
        const std::lock_guard callback_lock{owner_.call_callback_mutex_};

        try {
            owner_.on_call_media_state(*this);
        } catch (const pj::Error &error) {
            owner_.on_call_media_callback_error(error.info());
        } catch (const std::exception &error) {
            owner_.on_call_media_callback_error(error.what());
        } catch (...) {
            owner_.on_call_media_callback_error("unknown incoming call media callback failure");
        }
    }

  private:
    SipIncomingCallClient &owner_;
};

SipIncomingCallClient::SipIncomingCallClient(SipEndpointConfig endpoint_config,
                                             SipAccountConfig account_config)
    : SipIncomingCallClient(endpoint_config, std::move(account_config), {})
{}

SipIncomingCallClient::SipIncomingCallClient(SipEndpointConfig endpoint_config,
                                             SipAccountConfig account_config,
                                             SipCallAudioConfig audio_config)
    : endpoint_config_(endpoint_config), account_config_(std::move(account_config)),
      audio_config_(std::move(audio_config))
{}

SipIncomingCallClient::~SipIncomingCallClient()
{
    stop();
}

void SipIncomingCallClient::start()
{
    if (started_) {
        throw std::logic_error("SIP incoming call client is already started");
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

        account_ = std::make_unique<IncomingAccount>(*this);
        account_->create(pjsua2_account_config);
        started_ = true;
    } catch (...) {
        stop();
        throw;
    }
}

void SipIncomingCallClient::stop() noexcept
{
    hangup();

    {
        const std::lock_guard incoming_callback_lock{incoming_callback_mutex_};
        const std::lock_guard call_callback_lock{call_callback_mutex_};
        const std::lock_guard call_object_lock{call_object_mutex_};
        call_.reset();
    }

    static_cast<void>(wait_for_call_cleanup(std::chrono::seconds{2}));
    static_cast<void>(shutdown_account_safely());
    account_.reset();

    if (endpoint_ != nullptr && library_created_) {
        try {
            endpoint_->libDestroy();
        } catch (...) {
            std::fputs("PJSIP cleanup failed during incoming call shutdown\n", stderr);
        }
    }

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
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (pjsua_call_get_count() != 0U) {
        if (std::chrono::steady_clock::now() >= deadline) {
            std::fputs("PJSIP call slot remained busy during incoming call shutdown\n", stderr);
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }

    return true;
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

        call_ = std::make_unique<TrackedIncomingCall>(account, *this, call_id);
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
