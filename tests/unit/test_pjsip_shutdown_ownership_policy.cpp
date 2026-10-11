#include "pjsip/CallbackActivityGate.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <latch>
#include <limits>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

namespace {

using voip::pjsip::detail::CallbackActivityGate;

void require(const bool condition, const std::string_view message)
{
    if (!condition) {
        throw std::runtime_error(std::string{message});
    }
}

// Test-only proposal. In production, a PJSUA2 callback-quiescence receipt
// cannot be manufactured from the local CallbackActivityGate alone.
enum class Check : std::uint8_t
{
    CallSlots = 0,
    AccountShutdown = 1,
    LocalCallbacks = 2,
    ExternalCallbacks = 3
};

enum class Result : std::uint8_t
{
    Verified,
    Timeout,
    Busy,
    Exception,
    NotQuiescent
};

enum class State : std::uint8_t
{
    Retained,
    Verifying,
    Failed,
    Released
};

// A verification belongs to exactly one shutdown attempt. Failures are
// terminal for that attempt; a new attempt must re-verify every condition.
// This models a possible policy, NOT a guarantee supplied by PJSUA2.
class ShutdownOwnershipPolicy final
{
  public:
    using Generation = std::uint64_t;

    [[nodiscard]] std::optional<Generation> begin_attempt()
    {
        const std::lock_guard lock{mutex_};
        if (state_ == State::Released || generation_ == std::numeric_limits<Generation>::max()) {
            return std::nullopt;
        }
        ++generation_;
        verified_.fill(false);
        failure_.reset();
        state_ = State::Verifying;
        return generation_;
    }

    [[nodiscard]] bool report(const Generation generation, const Check check, const Result result)
    {
        const std::lock_guard lock{mutex_};
        if (generation != generation_ || state_ != State::Verifying) {
            return false;
        }
        const auto index = static_cast<std::size_t>(check);
        if (verified_[index]) {
            return false;
        }
        if (result != Result::Verified) {
            failure_ = result;
            state_ = State::Failed;
            return true;
        }
        verified_[index] = true;
        return true;
    }

    [[nodiscard]] bool release_if_verified(const Generation generation)
    {
        const std::lock_guard lock{mutex_};
        if (generation != generation_ || state_ != State::Verifying) {
            return false;
        }
        if (!std::all_of(verified_.begin(), verified_.end(),
                         [](const bool value) { return value; })) {
            return false;
        }
        require(call_owned_ && account_owned_ && endpoint_owned_, "partial modeled ownership");
        call_owned_ = false;
        release_log_[release_count_++] = "call";
        account_owned_ = false;
        release_log_[release_count_++] = "account";
        endpoint_owned_ = false;
        release_log_[release_count_++] = "endpoint";
        state_ = State::Released;
        return true;
    }

    [[nodiscard]] State state() const
    {
        const std::lock_guard lock{mutex_};
        return state_;
    }

    [[nodiscard]] std::optional<Result> failure() const
    {
        const std::lock_guard lock{mutex_};
        return failure_;
    }

    [[nodiscard]] bool owners_retained() const
    {
        const std::lock_guard lock{mutex_};
        return call_owned_ && account_owned_ && endpoint_owned_ && release_count_ == 0;
    }

    [[nodiscard]] std::array<std::string_view, 3> release_log() const
    {
        const std::lock_guard lock{mutex_};
        return release_log_;
    }

  private:
    mutable std::mutex mutex_;
    Generation generation_{0};
    std::array<bool, 4> verified_{};
    State state_{State::Retained};
    std::optional<Result> failure_;
    bool call_owned_{true};
    bool account_owned_{true};
    bool endpoint_owned_{true};
    std::array<std::string_view, 3> release_log_{};
    std::size_t release_count_{0};
};

constexpr std::array<Check, 4> all_checks{Check::CallSlots, Check::AccountShutdown,
                                          Check::LocalCallbacks, Check::ExternalCallbacks};

using Generation = ShutdownOwnershipPolicy::Generation;

// Model attempts start at generation 1. Accessing an absent optional is a
// test failure, not an unchecked dereference, even across helper boundaries.
[[nodiscard]] Generation begin_required_attempt(ShutdownOwnershipPolicy &policy)
{
    const Generation generation = policy.begin_attempt().value_or(0);
    require(generation != 0, "shutdown attempt unavailable");
    return generation;
}

void report_all(ShutdownOwnershipPolicy &policy, const Generation generation)
{
    for (const auto check : all_checks) {
        require(policy.report(generation, check, Result::Verified), "valid check rejected");
    }
}

void unverified_attempt_cannot_release()
{
    ShutdownOwnershipPolicy policy;
    require(policy.owners_retained(), "new policy must own all resources");
    const Generation attempt = begin_required_attempt(policy);
    require(!policy.release_if_verified(attempt), "empty evidence released resources");
    require(policy.state() == State::Verifying && policy.owners_retained(),
            "pending attempt failed to retain resources");
}

void each_missing_confirmation_blocks_release()
{
    for (const auto missing : all_checks) {
        ShutdownOwnershipPolicy policy;
        const Generation attempt = begin_required_attempt(policy);
        for (const auto check : all_checks) {
            if (check != missing) {
                require(policy.report(attempt, check, Result::Verified), "valid check rejected");
            }
        }
        require(!policy.release_if_verified(attempt), "missing confirmation authorized release");
        require(policy.owners_retained(), "missing confirmation lost resource ownership");
    }
}

void timeout_busy_exception_and_callback_failures_retain_owners()
{
    constexpr std::array failures{Result::Timeout, Result::Busy, Result::Exception,
                                  Result::NotQuiescent};
    for (const auto failure : failures) {
        for (const auto check : all_checks) {
            ShutdownOwnershipPolicy policy;
            const Generation attempt = begin_required_attempt(policy);
            require(policy.report(attempt, check, failure), "failure receipt rejected");
            require(policy.state() == State::Failed && policy.failure() == failure,
                    "failed attempt was not recorded");
            require(!policy.release_if_verified(attempt), "failure authorized release");
            require(policy.owners_retained(), "failure destroyed a modeled owner");
            require(!policy.report(attempt, check, Result::Verified),
                    "late success overwrote terminal failure");
        }
    }
}

void duplicate_success_cannot_override_a_receipt()
{
    ShutdownOwnershipPolicy policy;
    const Generation attempt = begin_required_attempt(policy);
    require(policy.report(attempt, Check::CallSlots, Result::Verified), "first success rejected");
    require(!policy.report(attempt, Check::CallSlots, Result::Verified),
            "duplicate receipt was silently accepted");
    require(!policy.release_if_verified(attempt), "partial receipt set authorized teardown");
    require(policy.owners_retained(), "duplicate receipt caused a partial release");
}

void restart_does_not_combine_successes_from_different_attempts()
{
    ShutdownOwnershipPolicy policy;
    const Generation first = begin_required_attempt(policy);
    require(policy.report(first, Check::CallSlots, Result::Verified), "slot receipt rejected");
    require(policy.report(first, Check::AccountShutdown, Result::Verified),
            "account receipt rejected");
    const Generation second = begin_required_attempt(policy);
    require(second != first, "retry did not advance generation");
    require(policy.report(second, Check::LocalCallbacks, Result::Verified),
            "new local callback receipt rejected");
    require(policy.report(second, Check::ExternalCallbacks, Result::Verified),
            "new external callback receipt rejected");
    require(!policy.release_if_verified(first), "stale generation authorized release");
    require(!policy.release_if_verified(second), "mixed generations authorized release");
    require(policy.owners_retained(), "mixed generations destroyed owners");
}

void stale_failure_and_stale_success_are_both_rejected()
{
    ShutdownOwnershipPolicy policy;
    const Generation first = begin_required_attempt(policy);
    const Generation second = begin_required_attempt(policy);
    require(first != second, "missing generation rollover");
    require(!policy.report(first, Check::AccountShutdown, Result::Busy),
            "stale failure poisoned retry");
    require(!policy.report(first, Check::ExternalCallbacks, Result::Verified),
            "stale success crossed retry boundary");
    require(policy.state() == State::Verifying && !policy.failure(),
            "stale completion changed current attempt state");
    require(policy.owners_retained(), "stale completion affected owners");
}

void failed_attempt_requires_an_explicit_retry()
{
    ShutdownOwnershipPolicy policy;
    const Generation first = begin_required_attempt(policy);
    require(policy.report(first, Check::CallSlots, Result::Timeout), "timeout receipt rejected");
    for (const auto check : all_checks) {
        require(!policy.report(first, check, Result::Verified),
                "terminal attempt unexpectedly accepted a new receipt");
    }
    require(!policy.release_if_verified(first), "failed attempt authorized teardown");
    require(policy.owners_retained(), "failed attempt destroyed a modeled owner");
    const Generation retry = begin_required_attempt(policy);
    require(retry != first, "explicit retry missing");
    report_all(policy, retry);
    require(policy.release_if_verified(retry), "fully reverified retry did not release");
}

void local_gate_drain_is_not_external_quiescence()
{
    CallbackActivityGate gate;
    ShutdownOwnershipPolicy policy;
    auto admitted = gate.try_enter();
    require(admitted.has_value(), "local callback was not admitted");
    gate.close();
    const Generation first = begin_required_attempt(policy);
    require(policy.report(first, Check::CallSlots, Result::Verified), "slot receipt rejected");
    require(policy.report(first, Check::AccountShutdown, Result::Verified),
            "account receipt rejected");
    require(!gate.closed_and_empty(), "active local callback was not detected");
    require(policy.report(first, Check::LocalCallbacks, Result::NotQuiescent),
            "local callback failure rejected");
    admitted.reset();
    require(gate.closed_and_empty(), "local callback failed to drain");
    const Generation retry = begin_required_attempt(policy);
    for (const auto check : all_checks) {
        if (check != Check::ExternalCallbacks) {
            require(policy.report(retry, check, Result::Verified), "retry receipt rejected");
        }
    }
    require(!policy.release_if_verified(retry), "local drain implied external quiescence");
    require(policy.owners_retained(), "external guarantee missing but owners were released");
}

void every_confirmation_order_releases_exactly_once()
{
    auto order = all_checks;
    std::size_t permutations = 0;
    do {
        ShutdownOwnershipPolicy policy;
        const Generation attempt = begin_required_attempt(policy);
        for (const auto check : order) {
            require(policy.report(attempt, check, Result::Verified), "success receipt rejected");
        }
        require(policy.release_if_verified(attempt), "fully verified release failed");
        require(policy.state() == State::Released, "successful release state missing");
        const std::array<std::string_view, 3> expected_order{"call", "account", "endpoint"};
        require(policy.release_log() == expected_order, "modeled teardown order was violated");
        require(!policy.release_if_verified(attempt), "second release repeated destruction");
        require(!policy.begin_attempt(), "released policy restarted shutdown");
        require(!policy.report(attempt, Check::CallSlots, Result::Verified),
                "released policy accepted a late completion");
        ++permutations;
    } while (std::next_permutation(order.begin(), order.end()));
    require(permutations == 24, "not all 24 confirmation orders were examined");
}

void delayed_callback_completion_cannot_cross_retry_epoch()
{
    ShutdownOwnershipPolicy policy;
    const Generation first = begin_required_attempt(policy);
    std::latch callback_parked{1};
    std::latch permit_callback_return{1};
    bool late_accepted = true;
    std::jthread delayed_callback{[&] {
        callback_parked.count_down();
        permit_callback_return.wait();
        late_accepted = policy.report(first, Check::ExternalCallbacks, Result::Verified);
    }};
    callback_parked.wait();
    const Generation retry = begin_required_attempt(policy);
    require(retry != first, "retry generation was not advanced");
    permit_callback_return.count_down();
    delayed_callback.join();
    require(!late_accepted, "delayed completion crossed generation boundary");
    require(!policy.release_if_verified(retry), "late callback authorized release");
    require(policy.owners_retained(), "retry did not retain owner objects");
    report_all(policy, retry);
    require(policy.release_if_verified(retry), "new attempt failed to verify ownership");
}

} // namespace

int main()
{
    try {
        unverified_attempt_cannot_release();
        each_missing_confirmation_blocks_release();
        timeout_busy_exception_and_callback_failures_retain_owners();
        duplicate_success_cannot_override_a_receipt();
        restart_does_not_combine_successes_from_different_attempts();
        stale_failure_and_stale_success_are_both_rejected();
        failed_attempt_requires_an_explicit_retry();
        local_gate_drain_is_not_external_quiescence();
        every_confirmation_order_releases_exactly_once();
        delayed_callback_completion_cannot_cross_retry_epoch();
        std::cout << "PASS: ten synthetic PJSIP shutdown ownership-policy scenarios "
                     "(no live PJSUA2 or callback quiescence proof)\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
