// Phase 8 P8-R3B: deterministic, NON-VENDOR lookup-to-gate lifetime model.
// No raw-pointer dereference, no PJPROJECT instrumentation and no SIP traffic.
#include "pjsip/CallbackActivityGate.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <latch>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace {

using voip::pjsip::detail::CallbackActivityGate;

void require(bool condition, std::string_view message)
{
    if (!condition) {
        throw std::runtime_error(std::string{message});
    }
}

// A *synthetic* per-call external ingress fence. It is deliberately absent
// from production: its job is to state and test a required lifetime contract.
// PJPROJECT's Call::lookup() is not proven to acquire a matching ticket.
class SyntheticDispatchFence final
{
  public:
    class Ticket final
    {
      public:
        Ticket(const Ticket &) = delete;
        Ticket &operator=(const Ticket &) = delete;
        Ticket(Ticket &&other) noexcept : fence_(std::exchange(other.fence_, nullptr)) {}
        Ticket &operator=(Ticket &&other) noexcept
        {
            if (this != &other) {
                release();
                fence_ = std::exchange(other.fence_, nullptr);
            }
            return *this;
        }
        ~Ticket()
        {
            release();
        }
        void release() noexcept
        {
            if (auto *fence = std::exchange(fence_, nullptr)) {
                fence->leave();
            }
        }

      private:
        friend class SyntheticDispatchFence;
        explicit Ticket(SyntheticDispatchFence *fence) noexcept : fence_(fence) {}
        SyntheticDispatchFence *fence_;
    };

    [[nodiscard]] std::optional<Ticket> enter()
    {
        const std::lock_guard lock{mutex_};
        if (closed_) {
            return std::nullopt;
        }
        ++active_;
        return Ticket{this};
    }

    void close() noexcept
    {
        const std::lock_guard lock{mutex_};
        closed_ = true;
    }

    [[nodiscard]] bool drained() const noexcept
    {
        const std::lock_guard lock{mutex_};
        return closed_ && active_ == 0;
    }

    [[nodiscard]] std::size_t active() const noexcept
    {
        const std::lock_guard lock{mutex_};
        return active_;
    }

  private:
    void leave() noexcept
    {
        const std::lock_guard lock{mutex_};
        assert(active_ != 0);
        --active_;
    }
    mutable std::mutex mutex_;
    std::size_t active_{0};
    bool closed_{false};
};

// A weak_ptr is used as a SAFE stand-in for PJSUA2's borrowed raw Call*.
// The test detects lifetime expiration but NEVER dereferences an expired object.
struct SimulatedCall final
{
    int marker{7};
};

void local_gate_cannot_protect_the_lookup_to_gate_gap()
{
    auto owned_call = std::make_shared<SimulatedCall>();
    std::weak_ptr<SimulatedCall> borrowed = owned_call;
    CallbackActivityGate local_gate;
    std::latch looked_up{1};
    std::latch proceed{1};
    std::atomic<bool> observed_retired_target{false};
    std::atomic<bool> local_admitted{true};

    std::jthread callback{[&] {
        // Simulated Call::lookup(): this non-owning handle does not retain Call.
        // Preserve a separate non-owning lookup snapshot.
        // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
        const auto lookup_result = borrowed;
        looked_up.count_down();
        proceed.wait();
        observed_retired_target.store(lookup_result.expired(), std::memory_order_relaxed);
        auto entry = local_gate.enter();
        local_admitted.store(entry.admitted(), std::memory_order_relaxed);
        // No dereference of lookup_result (before or after retirement).
    }};

    looked_up.wait();
    local_gate.close();
    const bool empty_gate_before_entry =
        local_gate.wait_for_empty_for(std::chrono::milliseconds{0});
    // Simulate an unprotected release based ONLY on local gate/call-slot state.
    owned_call.reset();
    const bool retired_before_entry = borrowed.expired();
    proceed.count_down();
    callback.join();
    require(empty_gate_before_entry, "local gate should drain before callback override entry");
    require(retired_before_entry, "simulated call should retire before callback override entry");
    require(observed_retired_target.load(std::memory_order_relaxed),
            "model did not expose pre-gate target retirement window");
    require(!local_admitted.load(std::memory_order_relaxed),
            "closed gate unexpectedly admitted post-lookup callback");
}

void external_ingress_fence_blocks_retirement_until_dispatch_returns()
{
    auto owned_call = std::make_shared<SimulatedCall>();
    std::weak_ptr<SimulatedCall> borrowed = owned_call;
    CallbackActivityGate local_gate;
    SyntheticDispatchFence external_fence;
    std::latch looked_up{1};
    std::latch proceed{1};
    std::atomic<bool> saw_live_target{false};
    std::atomic<bool> local_admitted{true};

    std::jthread callback{[&] {
        auto lifetime_ticket = external_fence.enter();
        // Preserve a separate non-owning lookup snapshot.
        // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
        const auto lookup_result = borrowed;
        looked_up.count_down();
        proceed.wait();
        saw_live_target.store(!lookup_result.expired(), std::memory_order_relaxed);
        auto entry = local_gate.enter();
        local_admitted.store(entry.admitted(), std::memory_order_relaxed);
        // Lifetime ticket exits AFTER all synthetic dispatch work returns.
    }};

    looked_up.wait();
    local_gate.close();
    external_fence.close();
    const bool local_empty = local_gate.closed_and_empty();
    const bool external_active = external_fence.active() == 1;
    const bool external_blocks = !external_fence.drained();
    const bool rejects_new_ingress = !external_fence.enter();
    // Crucially DO NOT reset owned_call while the synthetic ticket is active.
    proceed.count_down();
    callback.join();
    require(local_empty, "local gate should be empty in pre-gate window");
    require(external_active, "dispatch ticket not held across lookup");
    require(external_blocks, "external fence permitted premature release");
    require(rejects_new_ingress, "external fence accepted new callback after close");
    require(saw_live_target.load(std::memory_order_relaxed),
            "synthetic owner unexpectedly retired before external callback exit");
    require(!local_admitted.load(std::memory_order_relaxed),
            "local gate admitted a post-close callback");
    require(external_fence.drained(), "external callback did not drain");
    owned_call.reset();
    require(borrowed.expired(), "call was not retired after both boundaries drained");
}

void per_call_fences_do_not_block_later_calls()
{
    SyntheticDispatchFence old_call;
    SyntheticDispatchFence new_call;
    auto old_ticket = old_call.enter();
    require(old_ticket.has_value(), "old call could not begin");
    old_call.close();
    require(!old_call.drained(), "old callback is still in flight");
    auto next_ticket = new_call.enter();
    require(next_ticket.has_value(), "new call was incorrectly blocked by old fence");
    old_ticket.reset();
    require(old_call.drained(), "old call did not drain");
    new_call.close();
    require(!new_call.drained(), "new callback incorrectly marked drained");
    next_ticket.reset();
    require(new_call.drained(), "new callback did not drain");
}

void failed_shutdown_or_unfinished_call_slots_still_block_retirement()
{
    CallbackActivityGate gate;
    SyntheticDispatchFence external_fence;
    gate.close();
    external_fence.close();
    struct RetirementConditions final
    {
        bool slots_drained;
        bool account_shutdown_ok;
    };
    const auto may_retire = [&](const RetirementConditions conditions) {
        return conditions.slots_drained && conditions.account_shutdown_ok &&
               gate.closed_and_empty() && external_fence.drained();
    };
    require(!may_retire({false, true}), "call slots were ignored");
    require(!may_retire({true, false}), "failed account shutdown was ignored");
    require(!may_retire({false, false}), "both shutdown failures were ignored");
    require(may_retire({true, true}), "fully drained synthetic model should allow retirement");
}

} // namespace

int main()
{
    try {
        local_gate_cannot_protect_the_lookup_to_gate_gap();
        external_ingress_fence_blocks_retirement_until_dispatch_returns();
        per_call_fences_do_not_block_later_calls();
        failed_shutdown_or_unfinished_call_slots_still_block_retirement();
        std::cout << "P8R3B_MODEL_LOOKUP_TO_GATE_GAP=OBSERVED\n"
                     "P8R3B_MODEL_EXTERNAL_FENCE=PASS\n"
                     "P8R3B_MODEL_PER_CALL_REUSE=PASS\n"
                     "P8R3B_VENDOR_DISPATCH_INSTRUMENTED=NO\n"
                     "P8R3B_EXTERNAL_CALLBACK_QUIESCENCE=UNPROVEN\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL: P8-R3B synthetic lifetime model: " << error.what() << '\n';
        return 1;
    }
}
