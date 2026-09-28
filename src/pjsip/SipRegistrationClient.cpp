#include "pjsip/SipRegistrationClient.hpp"

#include <cstdio>
#include <stdexcept>
#include <utility>

namespace voip::pjsip {

class SipRegistrationClient::RegistrationAccount final : public pj::Account
{
  public:
    explicit RegistrationAccount(SipRegistrationClient &owner) : owner_(owner) {}

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

  private:
    SipRegistrationClient &owner_;
};

SipRegistrationClient::SipRegistrationClient(SipEndpointConfig endpoint_config,
                                             SipAccountConfig account_config)
    : endpoint_config_(endpoint_config), account_config_(std::move(account_config))
{}

SipRegistrationClient::~SipRegistrationClient()
{
    stop();
}

void SipRegistrationClient::start()
{
    if (started_) {
        throw std::logic_error("SIP registration client is already started");
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

        account_ = std::make_unique<RegistrationAccount>(*this);
        account_->create(pjsua2_account_config);
        started_ = true;
    } catch (...) {
        stop();
        throw;
    }
}

void SipRegistrationClient::stop() noexcept
{
    account_.reset();

    if (endpoint_ != nullptr && library_created_) {
        try {
            endpoint_->libDestroy();
        } catch (...) {
            std::fputs("PJSIP cleanup failed during shutdown\n", stderr);
        }
    }

    endpoint_.reset();
    library_created_ = false;
    started_ = false;
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
