#include <pjsua-lib/pjsua.h>
#include <pjsua2.hpp>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

using namespace std::chrono_literals;

namespace {

class Activity final
{
  public:
    explicit Activity(std::atomic<unsigned> &active) noexcept : active_(active)
    {
        active_.fetch_add(1U, std::memory_order_acq_rel);
    }

    ~Activity()
    {
        active_.fetch_sub(1U, std::memory_order_acq_rel);
    }

    Activity(const Activity &) = delete;
    Activity &operator=(const Activity &) = delete;

  private:
    std::atomic<unsigned> &active_;
};

class ProbeAccount final : public pj::Account
{
  public:
    void onRegState(pj::OnRegStateParam &) override
    {
        Activity activity{callbacks_active};

        total_callbacks.fetch_add(1U, std::memory_order_acq_rel);

        if (shutdown_returned.load(std::memory_order_acquire)) {
            callbacks_after_shutdown.fetch_add(1U, std::memory_order_acq_rel);
        }

        try {
            const auto info = getInfo();
            registration_active.store(info.regIsActive, std::memory_order_release);
            registration_status.store(static_cast<int>(info.regStatus), std::memory_order_release);
        } catch (...) {
        }
    }

    std::atomic<bool> registration_active{false};
    std::atomic<int> registration_status{0};
    std::atomic<unsigned> callbacks_active{0U};
    std::atomic<unsigned> total_callbacks{0U};
    std::atomic<unsigned> callbacks_after_shutdown{0U};
    std::atomic<bool> shutdown_returned{false};
};

class ProbeCall final : public pj::Call
{
  public:
    explicit ProbeCall(pj::Account &account) : pj::Call(account) {}

    void onCallState(pj::OnCallStateParam &) override
    {
        Activity activity{callbacks_active};

        try {
            const auto info = getInfo();
            if (info.state == PJSIP_INV_STATE_DISCONNECTED) {
                disconnected.store(true, std::memory_order_release);
            }
        } catch (...) {
        }
    }

    std::atomic<bool> disconnected{false};
    std::atomic<unsigned> callbacks_active{0U};
};

template <typename Predicate>
bool wait_for(Predicate predicate, const std::chrono::milliseconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;

    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(10ms);
    }

    return predicate();
}

std::string require_environment(const char *name)
{
    const char *value = std::getenv(name);
    if (value == nullptr || value[0] == '\0') {
        throw std::runtime_error(std::string{"missing environment variable: "} + name);
    }
    return value;
}

} // namespace

int main()
{
    const auto username = require_environment("PHASE8_SIP_USER");
    const auto server = require_environment("PHASE8_SIP_SERVER");
    const auto target = require_environment("PHASE8_SIP_TARGET");
    const auto password = require_environment("VOIP_SIP_PASSWORD");

    const std::string identity_uri = "sip:" + username + "@" + server;
    const std::string registrar_uri = "sip:" + server + ":5060;transport=udp";
    const std::string target_uri = "sip:" + target + "@" + server + ":5060";

    pj::Endpoint endpoint;
    bool library_created = false;

    std::unique_ptr<ProbeAccount> account;
    std::unique_ptr<ProbeCall> call;

    auto cleanup = [&] {
        if (library_created) {
            try {
                endpoint.hangupAllCalls();
            } catch (...) {
            }

            static_cast<void>(wait_for([] { return pjsua_call_get_count() == 0U; }, 3s));
        }

        call.reset();

        if (account != nullptr) {
            try {
                if (account->isValid()) {
                    pj::AccountShutdownParam shutdown;
                    shutdown.force = false;
                    account->shutdown2(shutdown);
                }
            } catch (...) {
            }
        }

        if (library_created) {
            try {
                endpoint.libDestroy();
            } catch (...) {
            }
            library_created = false;
        }

        account.reset();
    };

    try {
        endpoint.libCreate();
        library_created = true;

        pj::EpConfig endpoint_config;
        endpoint_config.logConfig.msgLogging = false;
        endpoint_config.logConfig.level = 1;
        endpoint_config.logConfig.consoleLevel = 1;
        endpoint.libInit(endpoint_config);

        pj::TransportConfig transport_config;
        transport_config.port = 0;

        const auto transport_id = endpoint.transportCreate(PJSIP_TRANSPORT_UDP, transport_config);
        endpoint.libStart();

        // This is a control-plane lifecycle experiment. Avoid dependence on
        // the physical audio device while still creating a real PJSUA call.
        endpoint.audDevManager().setNullDev();

        pj::AccountConfig account_config;
        account_config.idUri = identity_uri;
        account_config.regConfig.registrarUri = registrar_uri;
        account_config.regConfig.registerOnAdd = true;
        account_config.sipConfig.transportId = transport_id;
        account_config.sipConfig.authCreds.emplace_back("digest", "*", username,
                                                        PJSIP_CRED_DATA_PLAIN_PASSWD, password);

        account = std::make_unique<ProbeAccount>();
        account->create(account_config);

        const bool registered = wait_for(
            [&] {
                return account->registration_active.load(std::memory_order_acquire) &&
                       account->registration_status.load(std::memory_order_acquire) == 200;
            },
            10s);

        std::cout << "REGISTERED=" << (registered ? "yes" : "no")
                  << " status=" << account->registration_status.load(std::memory_order_acquire)
                  << '\n';

        if (!registered) {
            throw std::runtime_error("registration did not reach SIP 200");
        }

        call = std::make_unique<ProbeCall>(*account);

        pj::CallOpParam make_call{true};
        call->makeCall(target_uri, make_call);

        const bool active_slot_seen = wait_for([] { return pjsua_call_get_count() > 0U; }, 2s);

        std::cout << "ACTIVE_CALL_SLOTS=" << pjsua_call_get_count() << '\n';

        if (!active_slot_seen) {
            throw std::runtime_error("no active PJSUA call slot observed");
        }

        pj::AccountShutdownParam shutdown;
        shutdown.force = false;

        bool saw_ebusy = false;

        try {
            account->shutdown2(shutdown);
            std::cout << "FIRST_SHUTDOWN=UNEXPECTED_SUCCESS\n";
        } catch (const pj::Error &error) {
            if (error.status == PJ_EBUSY) {
                saw_ebusy = true;
                std::cout << "FIRST_SHUTDOWN=PJ_EBUSY\n";
            } else {
                std::cout << "FIRST_SHUTDOWN=ERROR status=" << error.status << '\n';
                throw;
            }
        }

        if (!saw_ebusy) {
            throw std::runtime_error(
                "non-forced shutdown did not return PJ_EBUSY while call slot was active");
        }

        pj::CallOpParam hangup;
        call->hangup(hangup);

        const bool disconnected_and_drained = wait_for(
            [&] {
                return call->disconnected.load(std::memory_order_acquire) &&
                       call->callbacks_active.load(std::memory_order_acquire) == 0U;
            },
            5s);

        if (!disconnected_and_drained) {
            throw std::runtime_error("DISCONNECTED callback did not complete");
        }

        const bool zero_slots = wait_for([] { return pjsua_call_get_count() == 0U; }, 5s);

        if (!zero_slots) {
            throw std::runtime_error("PJSUA call slots did not reach zero");
        }

        std::cout << "CALL_SLOTS_AFTER_HANGUP=" << pjsua_call_get_count() << '\n';

        // Release the derived Call wrapper only after the DISCONNECTED callback
        // completed and PJSUA reports zero call slots.
        call.reset();

        account->shutdown2(shutdown);

        const auto active_at_return = account->callbacks_active.load(std::memory_order_acquire);
        account->shutdown_returned.store(true, std::memory_order_release);

        std::cout << "SECOND_SHUTDOWN=SUCCESS\n";
        std::cout << "ACCOUNT_CALLBACKS_ACTIVE_AT_RETURN=" << active_at_return << '\n';

        if (!wait_for(
                [&] { return account->callbacks_active.load(std::memory_order_acquire) == 0U; },
                2s)) {
            throw std::runtime_error("account callback did not drain after shutdown2 returned");
        }

        const auto callbacks_before_observation =
            account->total_callbacks.load(std::memory_order_acquire);

        std::this_thread::sleep_for(1500ms);

        const auto callbacks_after_observation =
            account->total_callbacks.load(std::memory_order_acquire);

        std::cout << "ACCOUNT_CALLBACKS_DURING_GRACE="
                  << (callbacks_after_observation - callbacks_before_observation) << '\n';
        std::cout << "POST_SHUTDOWN_CALLBACKS="
                  << account->callbacks_after_shutdown.load(std::memory_order_acquire) << '\n';

        // Keep the derived callback target alive through Endpoint teardown.
        endpoint.libDestroy();
        library_created = false;

        std::cout << "LIB_DESTROY=SUCCESS\n";
        std::cout << "POST_LIBDESTROY_CALLBACKS="
                  << account->callbacks_after_shutdown.load(std::memory_order_acquire) << '\n';

        account.reset();

        std::cout << "PROBE_RESULT=PASS\n";
        return 0;
    } catch (const pj::Error &error) {
        std::cerr << "PROBE_RESULT=FAIL pjsip_status=" << error.status << '\n';
        cleanup();
        return 1;
    } catch (const std::exception &error) {
        std::cerr << "PROBE_RESULT=FAIL reason=\"" << error.what() << "\"\n";
        cleanup();
        return 1;
    } catch (...) {
        std::cerr << "PROBE_RESULT=FAIL reason=\"unknown\"\n";
        cleanup();
        return 1;
    }
}
