#include "pjsip/Pjsua2SipEngine.hpp"

#include "pjsip/Pjsua2AudioDeviceManager.hpp"
#include "pjsip/Pjsua2CallAudioBridge.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <pjsua-lib/pjsua.h>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <utility>

namespace voip::pjsip {
namespace {

[[nodiscard]] bool contains_whitespace(const std::string_view value)
{
    return std::any_of(value.begin(), value.end(), [](const char character) {
        return std::isspace(static_cast<unsigned char>(character)) != 0;
    });
}

[[nodiscard]] std::string registration_failure_reason(const pj::OnRegStateParam &parameter)
{
    if (!parameter.reason.empty()) {
        return parameter.reason;
    }
    if (parameter.status != PJ_SUCCESS) {
        return "registration transport failure";
    }
    return "registration failed";
}

[[nodiscard]] std::string call_failure_reason(const pj::CallInfo &info)
{
    if (!info.lastReason.empty()) {
        return info.lastReason;
    }
    if (!info.stateText.empty()) {
        return info.stateText;
    }
    return "call failed";
}

} // namespace

class Pjsua2SipEngine::EngineAccount final : public pj::Account
{
  public:
    explicit EngineAccount(Pjsua2SipEngine &owner) : owner_(owner) {}

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
        const std::lock_guard incoming_lock{owner_.incoming_callback_mutex_};
        const std::lock_guard call_callback_lock{owner_.call_callback_mutex_};

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
    Pjsua2SipEngine &owner_;
};

class Pjsua2SipEngine::EngineCall final : public pj::Call
{
  public:
    EngineCall(pj::Account &account, Pjsua2SipEngine &owner, const bool incoming,
               const int call_id = PJSUA_INVALID_ID)
        : pj::Call(account, call_id), owner_(owner), incoming_(incoming)
    {}

    void onCallState(pj::OnCallStateParam & /*parameter*/) override
    {
        const std::lock_guard callback_lock{owner_.call_callback_mutex_};

        try {
            owner_.on_call_state(getInfo(), incoming_);
        } catch (const pj::Error &error) {
            owner_.on_call_callback_error(error.info());
        } catch (const std::exception &error) {
            owner_.on_call_callback_error(error.what());
        } catch (...) {
            owner_.on_call_callback_error("unknown call state callback failure");
        }
    }

    void onCallMediaState(pj::OnCallMediaStateParam & /*parameter*/) override
    {
        const std::lock_guard callback_lock{owner_.call_callback_mutex_};

        try {
            owner_.on_call_media_state(*this);
        } catch (const pj::Error &error) {
            owner_.on_call_callback_error("audio media error: " + error.info());
        } catch (const std::exception &error) {
            owner_.on_call_callback_error("audio media error: " + std::string{error.what()});
        } catch (...) {
            owner_.on_call_callback_error("unknown audio media callback failure");
        }
    }

  private:
    Pjsua2SipEngine &owner_;
    bool incoming_{false};
};

Pjsua2SipEngine::Pjsua2SipEngine(SipEndpointConfig endpoint_config, SipAccountConfig account_config)
    : Pjsua2SipEngine(endpoint_config, std::move(account_config), {})
{}

Pjsua2SipEngine::Pjsua2SipEngine(SipEndpointConfig endpoint_config, SipAccountConfig account_config,
                                 SipCallAudioConfig audio_config)
    : endpoint_config_(endpoint_config), account_config_(std::move(account_config)),
      audio_config_(std::move(audio_config))
{}

Pjsua2SipEngine::~Pjsua2SipEngine()
{
    stop();
}

void Pjsua2SipEngine::set_observer(application::SipEngineObserver *observer) noexcept
{
    const std::lock_guard lock{observer_mutex_};
    observer_ = observer;
}

void Pjsua2SipEngine::register_account()
{
    if (started_) {
        if (call_active_.load()) {
            throw std::logic_error("cannot refresh SIP registration while a call is active");
        }
        if (account_ == nullptr || !account_->isValid()) {
            throw std::logic_error("PJSUA2 SIP account is not available");
        }

        registered_.store(false);
        account_->setRegistration(true);
        return;
    }
    if (!account_config_.register_on_add) {
        throw std::invalid_argument("PJSUA2 SIP engine requires register_on_add=true");
    }
    if (account_config_.transport != endpoint_config_.transport) {
        throw std::invalid_argument("SIP account and endpoint transports must match");
    }

    account_config_.validate();
    suppress_callbacks_.store(false);
    registered_.store(false);
    call_active_.store(false);
    call_connected_.store(false);
    incoming_call_.store(false);
    hangup_requested_.store(false);

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

        account_ = std::make_unique<EngineAccount>(*this);
        account_->create(pjsua2_account_config);
        started_ = true;
    } catch (...) {
        stop();
        throw;
    }
}

void Pjsua2SipEngine::make_call(const std::string &destination)
{
    if (!started_ || account_ == nullptr) {
        throw std::logic_error("PJSUA2 SIP engine is not started");
    }
    if (!registered_.load()) {
        throw std::logic_error("SIP account must be registered before placing a call");
    }

    const auto uri = destination_uri(destination);

    {
        const std::lock_guard callback_lock{call_callback_mutex_};
        const std::lock_guard call_lock{call_object_mutex_};
        if (call_active_.load()) {
            throw std::logic_error("a SIP call is already active");
        }

        call_.reset();
        call_ = std::make_unique<EngineCall>(*account_, *this, false);
        call_active_.store(true);
        call_connected_.store(false);
        incoming_call_.store(false);
        hangup_requested_.store(false);
    }

    try {
        const std::lock_guard call_lock{call_object_mutex_};
        const pj::CallOpParam parameter{true};
        call_->makeCall(uri, parameter);
    } catch (...) {
        const std::lock_guard call_lock{call_object_mutex_};
        call_active_.store(false);
        call_.reset();
        throw;
    }
}

void Pjsua2SipEngine::answer_call()
{
    if (!started_) {
        throw std::logic_error("PJSUA2 SIP engine is not started");
    }
    if (!call_active_.load() || !incoming_call_.load()) {
        throw std::logic_error("no incoming call is waiting to be answered");
    }

    const std::lock_guard call_lock{call_object_mutex_};
    if (call_ == nullptr) {
        throw std::logic_error("incoming call object is not available");
    }

    pj::CallOpParam parameter;
    parameter.statusCode = PJSIP_SC_OK;
    call_->answer(parameter);
}

void Pjsua2SipEngine::hangup_call()
{
    if (!started_) {
        throw std::logic_error("PJSUA2 SIP engine is not started");
    }

    const std::lock_guard call_lock{call_object_mutex_};
    if (call_ == nullptr || !call_active_.load()) {
        throw std::logic_error("no active SIP call is available to hang up");
    }

    hangup_requested_.store(true);
    const pj::CallOpParam parameter;
    call_->hangup(parameter);
}

void Pjsua2SipEngine::stop() noexcept
{
    suppress_callbacks_.store(true);

    if (call_active_.load()) {
        try {
            const std::lock_guard call_lock{call_object_mutex_};
            if (call_ != nullptr) {
                const pj::CallOpParam parameter;
                call_->hangup(parameter);
            }
        } catch (...) {
            std::fputs("PJSIP call hangup failed during engine shutdown\n", stderr);
        }
    }

    {
        const std::lock_guard incoming_lock{incoming_callback_mutex_};
        const std::lock_guard callback_lock{call_callback_mutex_};
        const std::lock_guard call_lock{call_object_mutex_};
        call_.reset();
    }

    static_cast<void>(wait_for_call_cleanup());
    static_cast<void>(shutdown_account_safely());
    account_.reset();

    if (endpoint_ != nullptr && library_created_) {
        try {
            endpoint_->libDestroy();
        } catch (...) {
            std::fputs("PJSIP cleanup failed during engine shutdown\n", stderr);
        }
    }

    endpoint_.reset();
    registered_.store(false);
    call_active_.store(false);
    call_connected_.store(false);
    incoming_call_.store(false);
    hangup_requested_.store(false);
    library_created_ = false;
    started_ = false;
}

bool Pjsua2SipEngine::shutdown_account_safely() noexcept
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
        std::fputs("PJSIP account shutdown failed during engine cleanup\n", stderr);
        return false;
    }
}

bool Pjsua2SipEngine::wait_for_call_cleanup() noexcept
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
    while (pjsua_call_get_count() != 0U) {
        if (std::chrono::steady_clock::now() >= deadline) {
            std::fputs("PJSIP call slot remained busy during engine shutdown\n", stderr);
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }

    return true;
}

std::string Pjsua2SipEngine::destination_uri(const std::string &destination) const
{
    if (destination.empty()) {
        throw std::invalid_argument("call destination must not be empty");
    }
    if (contains_whitespace(destination)) {
        throw std::invalid_argument("call destination must not contain whitespace");
    }
    if (destination.starts_with("sip:") || destination.starts_with("sips:")) {
        return destination;
    }
    if (destination.find('@') != std::string::npos) {
        return "sip:" + destination;
    }

    return "sip:" + destination + "@" + account_config_.server + ":" +
           std::to_string(account_config_.server_port) +
           ";transport=" + std::string{to_string(account_config_.transport)};
}

void Pjsua2SipEngine::on_registration_state(const pj::OnRegStateParam &parameter,
                                            const pj::AccountInfo &info)
{
    if (suppress_callbacks_.load()) {
        return;
    }

    if (info.regIsActive) {
        if (!registered_.exchange(true)) {
            notify_registration_succeeded();
        }
        return;
    }

    const auto status_code = static_cast<int>(parameter.code);
    if (parameter.status != PJ_SUCCESS || status_code >= 200) {
        registered_.store(false);
        notify_registration_failed(registration_failure_reason(parameter));
    }
}

void Pjsua2SipEngine::on_registration_callback_error(std::string reason) noexcept
{
    registered_.store(false);
    notify_registration_failed(std::move(reason));
}

void Pjsua2SipEngine::on_incoming_call(pj::Account &account, const int call_id)
{
    std::string remote_uri;

    {
        const std::lock_guard call_lock{call_object_mutex_};
        if (call_active_.load()) {
            pj::Call rejected_call{account, call_id};
            pj::CallOpParam busy_parameter;
            busy_parameter.statusCode = PJSIP_SC_BUSY_HERE;
            rejected_call.answer(busy_parameter);
            return;
        }

        call_.reset();
        call_ = std::make_unique<EngineCall>(account, *this, true, call_id);
        remote_uri = call_->getInfo().remoteUri;

        call_active_.store(true);
        call_connected_.store(false);
        incoming_call_.store(true);
        hangup_requested_.store(false);

        pj::CallOpParam ringing_parameter;
        ringing_parameter.statusCode = PJSIP_SC_RINGING;
        call_->answer(ringing_parameter);
    }

    notify_incoming_call(std::move(remote_uri));
}

void Pjsua2SipEngine::on_incoming_callback_error(std::string reason) noexcept
{
    call_active_.store(false);
    incoming_call_.store(false);
    notify_call_failed(std::move(reason));
}

void Pjsua2SipEngine::on_call_state(const pj::CallInfo &info, const bool incoming)
{
    if (suppress_callbacks_.load()) {
        return;
    }

    if (!incoming && info.state == PJSIP_INV_STATE_EARLY) {
        notify_remote_ringing();
        return;
    }

    if (info.state == PJSIP_INV_STATE_CONFIRMED) {
        if (!call_connected_.exchange(true)) {
            notify_call_connected();
        }
        return;
    }

    if (info.state != PJSIP_INV_STATE_DISCONNECTED) {
        return;
    }

    const auto was_connected = call_connected_.exchange(false);
    const auto local_hangup = hangup_requested_.exchange(false);
    const auto was_incoming = incoming_call_.exchange(false);
    call_active_.store(false);

    if (was_connected || local_hangup || was_incoming) {
        notify_call_ended();
    } else {
        notify_call_failed(call_failure_reason(info));
    }
}

void Pjsua2SipEngine::on_call_media_state(pj::Call &call)
{
    if (endpoint_ == nullptr) {
        throw std::logic_error("PJSIP endpoint is not available for call audio");
    }

    if (active_audio_media_indices(call.getInfo()).empty()) {
        return;
    }

    connect_active_call_audio(call, endpoint_->audDevManager());
}

void Pjsua2SipEngine::on_call_callback_error(std::string reason) noexcept
{
    notify_call_failed(std::move(reason));
}

void Pjsua2SipEngine::notify_registration_succeeded() noexcept
{
    try {
        const std::lock_guard lock{observer_mutex_};
        if (observer_ != nullptr && !suppress_callbacks_.load()) {
            observer_->on_registration_succeeded();
        }
    } catch (...) {
        std::fputs("SIP observer failed while handling registration success\n", stderr);
    }
}

void Pjsua2SipEngine::notify_registration_failed(std::string reason) noexcept
{
    try {
        const std::lock_guard lock{observer_mutex_};
        if (observer_ != nullptr && !suppress_callbacks_.load()) {
            observer_->on_registration_failed(std::move(reason));
        }
    } catch (...) {
        std::fputs("SIP observer failed while handling registration failure\n", stderr);
    }
}

void Pjsua2SipEngine::notify_incoming_call(std::string peer) noexcept
{
    try {
        const std::lock_guard lock{observer_mutex_};
        if (observer_ != nullptr && !suppress_callbacks_.load()) {
            observer_->on_incoming_call(std::move(peer));
        }
    } catch (...) {
        std::fputs("SIP observer failed while handling incoming call\n", stderr);
    }
}

void Pjsua2SipEngine::notify_remote_ringing() noexcept
{
    try {
        const std::lock_guard lock{observer_mutex_};
        if (observer_ != nullptr && !suppress_callbacks_.load()) {
            observer_->on_remote_ringing();
        }
    } catch (...) {
        std::fputs("SIP observer failed while handling remote ringing\n", stderr);
    }
}

void Pjsua2SipEngine::notify_call_connected() noexcept
{
    try {
        const std::lock_guard lock{observer_mutex_};
        if (observer_ != nullptr && !suppress_callbacks_.load()) {
            observer_->on_call_connected();
        }
    } catch (...) {
        std::fputs("SIP observer failed while handling call connection\n", stderr);
    }
}

void Pjsua2SipEngine::notify_call_ended() noexcept
{
    try {
        const std::lock_guard lock{observer_mutex_};
        if (observer_ != nullptr && !suppress_callbacks_.load()) {
            observer_->on_call_ended();
        }
    } catch (...) {
        std::fputs("SIP observer failed while handling call end\n", stderr);
    }
}

void Pjsua2SipEngine::notify_call_failed(std::string reason) noexcept
{
    try {
        const std::lock_guard lock{observer_mutex_};
        if (observer_ != nullptr && !suppress_callbacks_.load()) {
            observer_->on_call_failed(std::move(reason));
        }
    } catch (...) {
        std::fputs("SIP observer failed while handling call failure\n", stderr);
    }
}

} // namespace voip::pjsip
