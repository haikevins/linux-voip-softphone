#include "pjsip/CallbackActivityGate.hpp"

#include <atomic>
#include <cassert>
#include <cstddef>
#include <iostream>
#include <latch>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace {

using voip::pjsip::detail::CallbackActivityGate;

void require(const bool ok, const std::string_view reason)
{
    if (!ok) {
        throw std::runtime_error(std::string{reason});
    }
}

// TEST-ONLY substitute for a PJSUA2 callback producer. A ticket represents a
// callback that has already crossed the EXTERNAL ingress boundary, regardless
// of whether the local CallbackActivityGate later admits it. In real PJSUA2,
// no equivalent stop-and-drain guarantee has been established here.
class SyntheticExternalCallbackSource final
{
  public:
    class Ticket final
    {
      public:
        Ticket(const Ticket &) = delete;
        Ticket &operator=(const Ticket &) = delete;

        Ticket(Ticket &&other) noexcept : source_(std::exchange(other.source_, nullptr)) {}

        Ticket &operator=(Ticket &&other) noexcept
        {
            if (this != &other) {
                release();
                source_ = std::exchange(other.source_, nullptr);
            }
            return *this;
        }

        ~Ticket()
        {
            release();
        }

        void release() noexcept
        {
            if (auto *source = std::exchange(source_, nullptr)) {
                source->leave();
            }
        }

      private:
        friend class SyntheticExternalCallbackSource;
        explicit Ticket(SyntheticExternalCallbackSource *source) noexcept : source_(source) {}
        SyntheticExternalCallbackSource *source_{nullptr};
    };

    [[nodiscard]] std::optional<Ticket> try_begin()
    {
        const std::lock_guard lock{mutex_};
        if (stopped_) {
            return std::nullopt;
        }
        ++active_;
        return Ticket{this};
    }

    void stop_admitting() noexcept
    {
        const std::lock_guard lock{mutex_};
        stopped_ = true;
    }

    [[nodiscard]] bool quiesced() const noexcept
    {
        const std::lock_guard lock{mutex_};
        return stopped_ && active_ == 0;
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
        // Every Ticket releases at most once, including after a move.
        assert(active_ != 0);
        --active_;
    }

    mutable std::mutex mutex_;
    std::size_t active_{0};
    bool stopped_{false};
};

// This is a TEST-ONLY candidate policy; neither real SIP client calls it.
[[nodiscard]] bool teardown_permitted(const bool call_slots_drained, const bool account_shutdown_ok,
                                      const CallbackActivityGate &gate,
                                      const SyntheticExternalCallbackSource &source)
{
    return call_slots_drained && account_shutdown_ok && gate.closed_and_empty() &&
           source.quiesced();
}

void an_empty_closed_gate_does_not_stop_external_entries()
{
    CallbackActivityGate gate;
    SyntheticExternalCallbackSource source;
    gate.close();
    require(gate.closed_and_empty(), "closed empty gate precondition failed");

    auto external = source.try_begin();
    require(external.has_value(), "external source unexpectedly stopped");
    require(!gate.try_enter(), "closed local gate admitted an external callback");
    require(source.active() == 1, "external callback was not counted");
    require(!teardown_permitted(true, true, gate, source),
            "closed gate alone permitted teardown while external callback was active");
    external.reset();
    require(!teardown_permitted(true, true, gate, source),
            "external callback source is still able to start new callbacks");
}

void stop_admission_is_not_inflight_drain()
{
    CallbackActivityGate gate;
    SyntheticExternalCallbackSource source;
    auto external = source.try_begin();
    require(external.has_value(), "missing external callback");
    gate.close();
    source.stop_admitting();
    require(!source.try_begin(), "external source accepted a post-stop callback");
    require(!source.quiesced(), "external stop falsely implied callback drain");
    require(!teardown_permitted(true, true, gate, source),
            "in-flight external callback did not block teardown");
    external.reset();
    require(source.quiesced(), "external callback did not drain");
    require(teardown_permitted(true, true, gate, source),
            "fully stopped and drained synthetic callback source blocked teardown");
}

void external_stop_does_not_close_local_callback_gate()
{
    CallbackActivityGate gate;
    SyntheticExternalCallbackSource source;
    source.stop_admitting();
    require(source.quiesced(), "fresh stopped external source should drain");
    require(!teardown_permitted(true, true, gate, source),
            "open local gate incorrectly permitted teardown");
    gate.close();
    require(teardown_permitted(true, true, gate, source), "closed and drained model failed");
}

void admitted_callback_blocks_even_after_external_stop()
{
    CallbackActivityGate gate;
    SyntheticExternalCallbackSource source;
    auto external = source.try_begin();
    auto admitted = gate.try_enter();
    require(external && admitted, "failed to admit callback at both boundaries");
    source.stop_admitting();
    gate.close();
    external.reset();
    require(source.quiesced(), "external callbacks should have drained");
    require(!gate.closed_and_empty(), "local callback was still active");
    require(!teardown_permitted(true, true, gate, source), "local permit did not block teardown");
    admitted.reset();
    require(teardown_permitted(true, true, gate, source),
            "both drains should permit model teardown");
}

void all_sixteen_precondition_combinations_fail_closed()
{
    unsigned permitted = 0;
    for (unsigned mask = 0; mask < 16; ++mask) {
        CallbackActivityGate gate;
        SyntheticExternalCallbackSource source;
        const bool slots_drained = (mask & 1U) != 0;
        const bool account_stopped = (mask & 2U) != 0;
        if ((mask & 4U) != 0) {
            gate.close();
        }
        if ((mask & 8U) != 0) {
            source.stop_admitting();
        }
        const bool can_release = teardown_permitted(slots_drained, account_stopped, gate, source);
        require(can_release == (mask == 15), "four-condition truth table violated");
        if (can_release) {
            ++permitted;
        }
    }
    require(permitted == 1, "only one fully-authorized combination may release");
}

void concurrent_external_entry_survives_gate_close_until_it_exits()
{
    CallbackActivityGate gate;
    SyntheticExternalCallbackSource source;
    std::latch entered{1};
    std::latch finish{1};
    std::atomic<bool> callback_admitted{false};
    std::atomic<bool> callback_entered{false};

    std::jthread callback{[&] {
        auto external = source.try_begin();
        callback_entered.store(external.has_value(), std::memory_order_relaxed);
        entered.count_down();
        finish.wait();
        callback_admitted.store(gate.try_enter().has_value(), std::memory_order_relaxed);
    }};

    entered.wait();
    gate.close();
    source.stop_admitting();
    const bool external_callback_is_active = !source.quiesced();
    const bool local_gate_is_empty = gate.closed_and_empty();
    const bool release_blocked = !teardown_permitted(true, true, gate, source);
    finish.count_down();
    callback.join();
    require(external_callback_is_active, "external callback crossed ingress before stop");
    require(local_gate_is_empty, "gate should have no admitted callbacks");
    require(release_blocked,
            "race: empty gate wrongly authorized destruction while external entry exists");
    require(callback_entered.load(std::memory_order_relaxed), "callback missed external ingress");
    require(!callback_admitted.load(std::memory_order_relaxed),
            "late callback crossed closed gate");
    require(teardown_permitted(true, true, gate, source),
            "model did not drain after callback exit");
}

void concurrent_admitted_callback_survives_stop_until_both_permits_exit()
{
    CallbackActivityGate gate;
    SyntheticExternalCallbackSource source;
    std::latch entered{1};
    std::latch finish{1};
    std::atomic<bool> admitted{false};

    std::jthread callback{[&] {
        auto external = source.try_begin();
        auto local = gate.try_enter();
        admitted.store(external.has_value() && local.has_value(), std::memory_order_relaxed);
        entered.count_down();
        finish.wait();
    }};

    entered.wait();
    gate.close();
    source.stop_admitting();
    const bool external_callback_is_active = !source.quiesced();
    const bool local_callback_is_active = !gate.closed_and_empty();
    const bool release_blocked = !teardown_permitted(true, true, gate, source);
    finish.count_down();
    callback.join();
    require(external_callback_is_active, "in-flight external callback was lost");
    require(local_callback_is_active, "in-flight local callback was lost");
    require(release_blocked, "callback overlap allowed teardown");
    require(admitted.load(std::memory_order_relaxed), "callback did not enter both boundaries");
    require(teardown_permitted(true, true, gate, source), "callback overlap never drained");
}

void partial_failure_blocks_release_after_both_drains()
{
    CallbackActivityGate gate;
    SyntheticExternalCallbackSource source;
    gate.close();
    source.stop_admitting();
    require(!teardown_permitted(false, true, gate, source), "call-slot timeout was ignored");
    require(!teardown_permitted(true, false, gate, source), "account shutdown failure was ignored");
    require(teardown_permitted(true, true, gate, source), "synthetic retry was rejected");
}

void stop_is_idempotent_and_tickets_drain_once()
{
    CallbackActivityGate gate;
    SyntheticExternalCallbackSource source;
    auto original = source.try_begin();
    require(original.has_value(), "missing source ticket");
    auto moved = std::move(original);
    source.stop_admitting();
    source.stop_admitting();
    gate.close();
    gate.close();
    require(source.active() == 1, "moving ticket changed external active count");
    require(!source.quiesced(), "moved ticket did not preserve callback lifetime");
    moved.reset();
    require(source.active() == 0 && source.quiesced(), "ticket did not drain exactly once");
    require(teardown_permitted(true, true, gate, source), "idempotent stopped source was blocked");
}

} // namespace

int main()
{
    try {
        an_empty_closed_gate_does_not_stop_external_entries();
        stop_admission_is_not_inflight_drain();
        external_stop_does_not_close_local_callback_gate();
        admitted_callback_blocks_even_after_external_stop();
        all_sixteen_precondition_combinations_fail_closed();
        concurrent_external_entry_survives_gate_close_until_it_exits();
        concurrent_admitted_callback_survives_stop_until_both_permits_exit();
        partial_failure_blocks_release_after_both_drains();
        stop_is_idempotent_and_tickets_drain_once();
        std::cout << "PASS: Phase 8 synthetic callback ingress race model (nine scenarios; live "
                     "PJSUA2 NOT TESTED)\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
