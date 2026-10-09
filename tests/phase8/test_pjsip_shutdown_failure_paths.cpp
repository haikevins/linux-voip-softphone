#include "pjsip/CallbackActivityGate.hpp"
#include "pjsip/PjsipCallSlotCleanup.hpp"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using namespace std::chrono_literals;
using voip::pjsip::detail::CallbackActivityGate;
using voip::pjsip::detail::wait_for_zero_call_slots;
using ClockPoint = std::chrono::steady_clock::time_point;

void require(const bool condition, const std::string_view message)
{
    if (!condition) {
        throw std::runtime_error(std::string{message});
    }
}

struct FakeClock
{
    ClockPoint time{};
    unsigned sleeps{0};

    void sleep(const std::chrono::milliseconds delay)
    {
        require(delay == 10ms, "call-slot polling must use 10ms intervals");
        time += delay;
        ++sleeps;
    }
};

// TEST-ONLY proposed fail-closed teardown policy. Both real Sip*CallClient::stop()
// implementations currently discard the two shutdown results; this model is
// intentionally NOT called by production, and cannot prove PJSUA2 quiescence.
struct TeardownModel
{
    CallbackActivityGate callbacks;
    bool external_callback_source_stopped{false};
    bool call_owned{true};
    bool account_owned{true};
    bool endpoint_owned{true};
    unsigned releases{0};
    std::string release_order;

    [[nodiscard]] bool try_release(const bool slots_drained, const bool account_stopped)
    {
        if (!slots_drained || !account_stopped || !external_callback_source_stopped ||
            !callbacks.closed_and_empty()) {
            return false;
        }
        if (endpoint_owned) {
            require(call_owned && account_owned, "partial resource lifetime detected");
            call_owned = false;
            release_order += "call ";
            account_owned = false;
            release_order += "account ";
            endpoint_owned = false;
            release_order += "endpoint";
            ++releases;
        }
        return true;
    }

    void require_retained() const
    {
        require(call_owned && account_owned && endpoint_owned,
                "failure must retain all three simulated owners");
        require(releases == 0 && release_order.empty(), "failure cannot partially release");
    }
};

void call_slot_timeout_retains_owners()
{
    FakeClock clock;
    unsigned probes = 0;
    const bool slots_drained = wait_for_zero_call_slots(
        25ms,
        [&] {
            ++probes;
            return 1U;
        },
        [&] { return clock.time; }, [&](auto delay) { clock.sleep(delay); });
    require(!slots_drained && probes == 4 && clock.sleeps == 3,
            "timeout probe must fail after a bounded wait");

    TeardownModel model;
    model.callbacks.close();
    model.external_callback_source_stopped = true; // Only a synthetic promise.
    require(!model.try_release(slots_drained, true), "timed-out slots must veto release");
    model.require_retained();
}

void account_busy_retains_owners()
{
    TeardownModel model;
    model.callbacks.close();
    model.external_callback_source_stopped = true;
    const bool simulated_shutdown2_busy = false; // Represents PJ_EBUSY, not a real API call.
    require(!model.try_release(true, simulated_shutdown2_busy),
            "failed account shutdown must veto release");
    model.require_retained();
}

void account_exception_retains_owners()
{
    TeardownModel model;
    model.callbacks.close();
    model.external_callback_source_stopped = true;
    bool shutdown_ok = false;
    try {
        throw std::runtime_error("synthetic shutdown2 exception");
    } catch (const std::exception &) {
        shutdown_ok = false;
    }
    require(!model.try_release(true, shutdown_ok), "shutdown exception must veto release");
    model.require_retained();
}

void admitted_callback_vetoes_zero_slot_cleanup()
{
    TeardownModel model;
    auto callback = model.callbacks.try_enter();
    require(callback.has_value(), "synthetic callback admission failed");
    model.callbacks.close();
    model.external_callback_source_stopped = true;
    require(!model.try_release(true, true), "in-flight callback must veto release");
    model.require_retained();
    callback.reset();
    require(model.callbacks.wait_for_empty_for(0ms), "model callback should drain");
    require(model.try_release(true, true), "drained model can release with all guarantees");
}

void open_gate_vetoes_cleanup()
{
    TeardownModel model;
    model.external_callback_source_stopped = true;
    require(!model.try_release(true, true), "open callback gate must veto release");
    model.require_retained();
}

void closed_gate_without_external_quiescence_vetoes_cleanup()
{
    TeardownModel model;
    model.callbacks.close();
    require(model.callbacks.closed_and_empty(), "model gate is not closed");
    require(!model.try_release(true, true), "gate alone is not external PJSUA2 quiescence");
    model.require_retained();
}

void combined_failures_stay_fail_closed()
{
    TeardownModel model;
    auto callback = model.callbacks.try_enter();
    require(callback.has_value(), "synthetic callback admission failed");
    model.callbacks.close();
    require(!model.try_release(false, false), "combined failures must veto release");
    model.require_retained();
    callback.reset();
    require(!model.try_release(false, true), "late callback drain cannot fix busy slots");
    model.require_retained();
    require(!model.try_release(true, false), "slot drain cannot fix account failure");
    model.require_retained();
    require(!model.try_release(true, true), "external callback source is not stopped");
    model.require_retained();
}

void explicit_synthetic_success_releases_in_order_once()
{
    TeardownModel model;
    model.callbacks.close();
    model.external_callback_source_stopped = true; // NOT confirmed by production.
    require(model.try_release(true, true), "all explicit model conditions should permit release");
    require(!model.call_owned && !model.account_owned && !model.endpoint_owned,
            "all simulated resources should be released");
    require(model.release_order == "call account endpoint", "teardown order changed");
    require(model.releases == 1, "simulated teardown must occur exactly once");
    require(model.try_release(true, true), "model release should be idempotent");
    require(model.releases == 1, "idempotent release repeated destruction");
}

void retry_requires_all_preconditions()
{
    TeardownModel model;
    model.callbacks.close();
    require(!model.try_release(false, false), "failure path must abort");
    model.require_retained();
    model.external_callback_source_stopped = true;
    require(!model.try_release(true, false), "account failure still blocks retry");
    model.require_retained();
    require(model.try_release(true, true), "fully resolved synthetic retry must succeed");
}

} // namespace

int main()
{
    try {
        call_slot_timeout_retains_owners();
        account_busy_retains_owners();
        account_exception_retains_owners();
        admitted_callback_vetoes_zero_slot_cleanup();
        open_gate_vetoes_cleanup();
        closed_gate_without_external_quiescence_vetoes_cleanup();
        combined_failures_stay_fail_closed();
        explicit_synthetic_success_releases_in_order_once();
        retry_requires_all_preconditions();
        std::cout << "PASS: nine synthetic PJSIP shutdown failure-path scenarios "
                     "(model only; real PJSUA2 not exercised)\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
