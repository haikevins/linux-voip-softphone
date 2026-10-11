#include "pjsip/PjsipAccountShutdownAttempt.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using voip::pjsip::detail::attempt_account_shutdown;

void require(bool condition, std::string_view explanation)
{
    if (!condition) {
        throw std::runtime_error(std::string{explanation});
    }
}

class SimulatedPjError : public std::runtime_error
{
  public:
    using std::runtime_error::runtime_error;
};

struct FakeAccount
{
    bool busy{false};
    bool unexpected_failure{false};
    bool owned{true};
    bool nonforced_seen{false};
    unsigned invocations{0};

    void shutdown2(bool force)
    {
        ++invocations;
        nonforced_seen = !force;
        require(!force, "the test must model nonforced shutdown");
        if (busy) {
            throw SimulatedPjError("simulated PJ_EBUSY");
        }
        if (unexpected_failure) {
            throw 42;
        }
    }
};

struct FakeEndpoint
{
    bool owned{true};
    unsigned destructions{0};
};

// Model of the release decision at the *call site*; production stop() retains
// its own real PJSUA2 ordering, verified separately by source regressions.
// These fakes cannot demonstrate vendor callback-source quiescence.
bool stop_model(FakeAccount &account, FakeEndpoint &endpoint, bool local_callbacks_drained,
                bool call_slots_drained, unsigned &api_failures, unsigned &other_failures)
{
    if (!local_callbacks_drained || !call_slots_drained) {
        return false;
    }
    const bool stopped = attempt_account_shutdown<SimulatedPjError>(
        [&] { account.shutdown2(false); },
        [&](const SimulatedPjError &) { ++api_failures; },
        [&] { ++other_failures; });
    if (!stopped) {
        return false;
    }
    require(account.owned && endpoint.owned, "model owners must still exist before release");
    ++endpoint.destructions;
    endpoint.owned = false;
    account.owned = false;
    return true;
}

void success_is_nonforced()
{
    FakeAccount account;
    FakeEndpoint endpoint;
    unsigned api_failures{}, other_failures{};
    require(stop_model(account, endpoint, true, true, api_failures, other_failures),
            "successful model shutdown must complete");
    require(account.invocations == 1 && account.nonforced_seen,
            "one nonforced operation must be dispatched");
    require(!account.owned && !endpoint.owned && endpoint.destructions == 1,
            "successful model may release owners once");
    require(api_failures == 0 && other_failures == 0, "no error should be logged");
}

void busy_failure_preserves_owners_until_explicit_retry()
{
    FakeAccount account;
    FakeEndpoint endpoint;
    unsigned api_failures{}, other_failures{};
    account.busy = true;
    require(!stop_model(account, endpoint, true, true, api_failures, other_failures),
            "simulated busy account must veto model release");
    require(account.owned && endpoint.owned && endpoint.destructions == 0,
            "busy account must retain modeled owners");
    require(account.invocations == 1 && api_failures == 1 && other_failures == 0,
            "busy result must be caught exactly once");

    // Merely clearing the busy flag does not trigger a background retry.
    account.busy = false;
    require(account.invocations == 1 && account.owned && endpoint.owned,
            "retry must be an explicit owner action");
    require(!stop_model(account, endpoint, true, false, api_failures, other_failures),
            "call slots must be checked again for each retry");
    require(account.invocations == 1, "blocked retry cannot issue shutdown2");
    require(stop_model(account, endpoint, true, true, api_failures, other_failures),
            "explicit retry may succeed after model conditions are satisfied");
    require(account.invocations == 2 && endpoint.destructions == 1,
            "explicit retry must not destroy twice");
}

void unknown_failure_is_fail_closed()
{
    FakeAccount account;
    FakeEndpoint endpoint;
    unsigned api_failures{}, other_failures{};
    account.unexpected_failure = true;
    require(!stop_model(account, endpoint, true, true, api_failures, other_failures),
            "unknown failure must veto model release");
    require(account.owned && endpoint.owned && endpoint.destructions == 0,
            "exception must retain modeled owners");
    require(api_failures == 0 && other_failures == 1,
            "unknown failure must use fallback handler");
}

void logging_failures_cannot_escape_or_authorize_release()
{
    const auto api_error = attempt_account_shutdown<SimulatedPjError>(
        [] { throw SimulatedPjError("simulated PJ_EBUSY"); },
        [](const SimulatedPjError &) { throw std::runtime_error("logging failed"); },
        [] { throw std::runtime_error("wrong handler"); });
    require(!api_error, "failed API-error logging cannot authorize shutdown");

    const auto unexpected_error = attempt_account_shutdown<SimulatedPjError>(
        [] { throw 1; }, [](const SimulatedPjError &) {},
        [] { throw std::runtime_error("fallback logging failed"); });
    require(!unexpected_error, "failed fallback logging cannot authorize shutdown");
}

void undrained_callbacks_and_call_slots_veto_operation()
{
    FakeAccount account;
    FakeEndpoint endpoint;
    unsigned api_failures{}, other_failures{};
    require(!stop_model(account, endpoint, false, true, api_failures, other_failures),
            "undrained local callbacks must veto model shutdown");
    require(!stop_model(account, endpoint, true, false, api_failures, other_failures),
            "remaining call slots must veto model shutdown");
    require(account.invocations == 0 && account.owned && endpoint.owned,
            "model preconditions must run before invoking shutdown2");
}

} // namespace

int main()
{
    try {
        success_is_nonforced();
        busy_failure_preserves_owners_until_explicit_retry();
        unknown_failure_is_fail_closed();
        logging_failures_cannot_escape_or_authorize_release();
        undrained_callbacks_and_call_slots_veto_operation();
        std::cout << "PASS: five shutdown-attempt injection scenarios "
                     "(shared production invocation helper; synthetic model only)\n";
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
