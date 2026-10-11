#include "pjsip/SipRegistrationClient.hpp"

#include "pjsip/CallbackActivityGate.hpp"
#include "pjsip/PjsipAccountShutdownAttempt.hpp"

#include <atomic>
#include <cstdio>
#include <exception>
#include <stdexcept>
#include <utility>

namespace voip::pjsip {

struct SipRegistrationClient::CallbackContext final
{
    explicit CallbackContext(SipRegistrationClient &owner) noexcept : owner_(&owner) {}

    [[nodiscard]] SipRegistrationClient *owner() const noexcept
    {
        return owner_.load(std::memory_order_acquire);
    }

    void detach_owner() noexcept
    {
        owner_.store(nullptr, std::memory_order_release);
    }

    detail::CallbackActivityGate account_activity;

  private:
    std::atomic<SipRegistrationClient *> owner_;
};

class SipRegistrationClient::RegistrationAccount final : public pj::Account
{
  public:
    explicit RegistrationAccount(std::shared_ptr<CallbackContext> context)
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

  private:
    std::shared_ptr<CallbackContext> context_;
};

SipRegistrationClient::SipRegistrationClient(SipEndpointConfig endpoint_config,
                                             SipAccountConfig account_config)
    : endpoint_config_(endpoint_config), account_config_(std::move(account_config)),
      callback_context_(std::make_shared<CallbackContext>(*this))
{}

SipRegistrationClient::~SipRegistrationClient()
{
    stop();
    if (library_created_ || endpoint_ != nullptr || account_ != nullptr) {
        std::fputs("PJSIP registration client teardown could not prove safe destruction\n", stderr);
        std::terminate();
    }
    callback_context_->detach_owner();
}

void SipRegistrationClient::start()
{
    if (started_) {
        throw std::logic_error("SIP registration client is already started");
    }
    if (library_created_ || endpoint_ != nullptr || account_ != nullptr) {
        throw std::logic_error("SIP registration client retains teardown state");
    }
    if (!callback_context_->account_activity.reset_if_empty()) {
        throw std::logic_error("SIP registration callback activity did not drain before restart");
    }
    if (!account_config_.register_on_add) {
        throw std::invalid_argument("SIP registration client requires register_on_add=true");
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

        auto pjsua2_account_config = account_config_.to_pjsua2_account_config();
        pjsua2_account_config.sipConfig.transportId = transport_id;

        account_ = std::make_unique<RegistrationAccount>(callback_context_);
        account_->create(pjsua2_account_config);
        started_ = true;
    } catch (...) {
        stop();
        throw;
    }
}

void SipRegistrationClient::stop() noexcept
{
    if (!library_created_) {
        if (account_ != nullptr) {
            std::fputs("PJSIP registration shutdown found an account without a library\n", stderr);
            return;
        }
        endpoint_.reset();
        started_ = false;
        return;
    }
    if (endpoint_ == nullptr) {
        std::fputs("PJSIP registration shutdown lost the endpoint owner\n", stderr);
        return;
    }

    callback_context_->account_activity.close();
    if (!callback_context_->account_activity.wait_for_empty_for(std::chrono::seconds{2})) {
        std::fputs("PJSIP registration callbacks did not drain before account shutdown\n", stderr);
        return;
    }

    if (!shutdown_account_safely()) {
        return;
    }

    if (!callback_context_->account_activity.wait_for_empty_for(std::chrono::seconds{2})) {
        std::fputs("PJSIP registration callbacks did not drain after account shutdown\n", stderr);
        return;
    }

    // Retain the derived Account callback target through libDestroy(). PJProject
    // 2.17 destroys its worker/media/endpoints inside this boundary and may
    // emit account callbacks while destroying the SIP endpoint. This ordering
    // is an implementation-reviewed lifetime guard, not a universal callback
    // quiescence proof.
    try {
        endpoint_->libDestroy();
    } catch (...) {
        std::fputs("PJSIP cleanup failed during registration shutdown\n", stderr);
        return;
    }

    account_.reset();
    endpoint_.reset();
    library_created_ = false;
    started_ = false;
}

bool SipRegistrationClient::shutdown_account_safely() noexcept
{
    if (account_ == nullptr || !account_->isValid()) {
        return true;
    }

    pj::AccountShutdownParam parameter;
    parameter.force = false;

    const bool shutdown_ok = detail::attempt_account_shutdown<pj::Error>(
        [&] { account_->shutdown2(parameter); },
        [&](const pj::Error &error) {
            std::fprintf(stderr, "PJSIP registration account shutdown failed: %s\n",
                         error.info().c_str());
        },
        [&] {
            std::fputs("PJSIP registration account shutdown failed during cleanup\n", stderr);
        });
    if (!shutdown_ok) {
        return false;
    }
    return true;
}

RegistrationResult
SipRegistrationClient::wait_for_registration(const std::chrono::milliseconds timeout)
{
    if (!started_) {
        throw std::logic_error("SIP registration client is not started");
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

void SipRegistrationClient::on_registration_state(const pj::OnRegStateParam &parameter,
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

void SipRegistrationClient::on_registration_callback_error(std::string reason) noexcept
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
        std::fputs("PJSIP registration callback error handling failed\n", stderr);
        registration_changed_.notify_all();
    }
}

} // namespace voip::pjsip
