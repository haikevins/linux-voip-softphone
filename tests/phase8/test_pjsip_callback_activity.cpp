#include "pjsip/CallbackActivityGate.hpp"

#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <latch>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

using voip::pjsip::detail::CallbackActivityGate;
using namespace std::chrono_literals;

namespace {
void require(bool condition, std::string_view reason)
{
    if (!condition) {
        throw std::runtime_error(std::string{reason});
    }
}

void no_close_does_not_authorize_cleanup()
{
    CallbackActivityGate gate;
    require(gate.admitted_callbacks() == 0, "fresh gate must be empty");
    require(!gate.closed_and_empty(), "open gate cannot authorize cleanup");
    require(!gate.wait_for_empty_for(0ms), "open gate must not report drained");
    require(!gate.wait_for_empty_for(-1ms), "negative timeout must fail closed");
}

void empty_closed_gate_drains()
{
    CallbackActivityGate gate;
    gate.close();
    gate.close();
    require(gate.closed_and_empty(), "closed empty gate should drain");
    require(gate.wait_for_empty_for(0ms), "closed empty gate should pass immediate wait");
    require(!gate.try_enter().has_value(), "close must reject new admission");
}

void active_permit_blocks_cleanup_even_if_call_slots_are_zero()
{
    CallbackActivityGate gate;
    auto permit = gate.try_enter();
    require(permit.has_value(), "callback permit admission failed");
    const unsigned simulated_pjsip_slots = 0;
    require(simulated_pjsip_slots == 0, "wrong test precondition");
    gate.close();
    require(!gate.closed_and_empty(), "zero slots != callbacks drained");
    require(!gate.wait_for_empty_for(0ms), "active permit must block cleanup");
    permit.reset();
    require(gate.wait_for_empty_for(0ms), "released permit must allow drain");
}

void timeouts_do_not_force_reclamation()
{
    CallbackActivityGate gate;
    auto permit = gate.try_enter();
    gate.close();
    require(!gate.wait_for_empty_for(20ms), "active permit should time out");
    require(!gate.closed_and_empty(), "timeout must not authorize cleanup");
    require(!gate.try_enter().has_value(), "gate must remain closed after timeout");
    permit.reset();
    require(gate.wait_for_empty_for(0ms), "late permit release must drain");
}

void moved_permit_releases_exactly_once()
{
    CallbackActivityGate gate;
    auto first = gate.try_enter();
    require(first.has_value(), "permit missing");
    auto second = std::move(first);
    require(gate.admitted_callbacks() == 1, "moving permit changed count");
    first = std::nullopt;
    require(gate.admitted_callbacks() == 1, "moved-from release counted twice");
    second.reset();
    require(gate.admitted_callbacks() == 0, "move-assigned permit leaked");
    gate.close();
    require(gate.closed_and_empty(), "movable permit did not drain");
}

void concurrent_wait_unblocks_after_last_callback()
{
    CallbackActivityGate gate;
    auto first = gate.try_enter();
    auto second = gate.try_enter();
    require(first && second, "failed to acquire callbacks");
    gate.close();

    std::promise<bool> promise;
    auto result = promise.get_future();
    std::latch started{1};
    std::jthread waiter{[&] {
        started.count_down();
        promise.set_value(gate.wait_for_empty_for(2s));
    }};
    started.wait();
    first.reset();
    require(!gate.closed_and_empty(), "drain released with callback still running");
    second.reset();
    require(result.wait_for(2s) == std::future_status::ready, "waiter stuck");
    require(result.get(), "drain failed after final callback completed");
}


void tracked_entry_after_close_is_counted_until_return()
{
    CallbackActivityGate gate;
    gate.close();
    auto entry = gate.enter();
    require(!entry.admitted(), "closed production entry must be rejected");
    require(gate.admitted_callbacks() == 1,
            "rejected production entry must remain counted until callback return");
    require(!gate.wait_for_empty_for(0ms),
            "tracked rejected entry must block drain while callback method is active");
    entry.release();
    require(gate.wait_for_empty_for(0ms),
            "tracked rejected entry must drain after callback method returns");
}

void reset_requires_empty_gate_then_reopens_admission()
{
    CallbackActivityGate gate;
    auto entry = gate.enter();
    require(entry.admitted(), "open production entry should be admitted");
    gate.close();
    require(!gate.reset_if_empty(), "active callback must block gate reset");
    entry.release();
    require(gate.reset_if_empty(), "drained gate should reset for a later start");
    auto permit = gate.try_enter();
    require(permit.has_value(), "reset gate did not reopen admission");
    permit.reset();
}

void concurrent_admission_then_close_rejects_late_entries()
{
    CallbackActivityGate gate;
    std::latch start{1};
    std::atomic<int> admitted{0};
    std::vector<std::jthread> threads;
    threads.reserve(4);
    for (int n = 0; n < 4; ++n) {
        threads.emplace_back([&] {
            start.wait();
            for (int i = 0; i < 500; ++i) {
                if (auto permit = gate.try_enter()) {
                    admitted.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }
    start.count_down();
    gate.close();
    for (auto &thread : threads) {
        thread.join();
    }
    require(gate.wait_for_empty_for(0ms), "callbacks did not drain");
    require(!gate.try_enter(), "late callback entered after closure");
    require(admitted.load() >= 0, "invalid admission counter");
}
} // namespace

int main()
{
    try {
        no_close_does_not_authorize_cleanup();
        empty_closed_gate_drains();
        active_permit_blocks_cleanup_even_if_call_slots_are_zero();
        timeouts_do_not_force_reclamation();
        moved_permit_releases_exactly_once();
        concurrent_wait_unblocks_after_last_callback();
        tracked_entry_after_close_is_counted_until_return();
        reset_requires_empty_gate_then_reopens_admission();
        concurrent_admission_then_close_rejects_late_entries();
        std::cout
            << "PASS: nine callback activity scenarios (admission, drain, reset, concurrency)\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
