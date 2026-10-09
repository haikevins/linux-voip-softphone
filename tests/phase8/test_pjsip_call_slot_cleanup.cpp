#include "pjsip/PjsipCallSlotCleanup.hpp"

#include <chrono>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using namespace std::chrono_literals;
using TimePoint = std::chrono::steady_clock::time_point;

void require(const bool condition, const std::string_view explanation)
{
    if (!condition) {
        throw std::runtime_error(std::string(explanation));
    }
}

struct FakeClock
{
    TimePoint time{};
    std::size_t sleeps{};

    [[nodiscard]] TimePoint now() const { return time; }
    void sleep(const std::chrono::milliseconds delay)
    {
        require(delay == 10ms, "polling delay must remain exactly 10ms");
        ++sleeps;
        time += delay;
    }
};

void test_already_drained_has_no_delay()
{
    FakeClock clock;
    unsigned count_calls = 0;
    const bool drained = voip::pjsip::detail::wait_for_zero_call_slots(
        2s, [&] { ++count_calls; return 0U; },
        [&] { return clock.now(); }, [&](auto delay) { clock.sleep(delay); });
    require(drained, "zero slots should succeed immediately");
    require(count_calls == 1 && clock.sleeps == 0, "no sleeping when zero");
}

void test_later_drain_succeeds_before_deadline()
{
    FakeClock clock;
    unsigned polls = 0;
    const bool drained = voip::pjsip::detail::wait_for_zero_call_slots(
        100ms, [&] { return (++polls < 4) ? 1U : 0U; },
        [&] { return clock.now(); }, [&](auto delay) { clock.sleep(delay); });
    require(drained, "slots that drain during polling should succeed");
    require(polls == 4 && clock.sleeps == 3 && clock.time == TimePoint{30ms},
            "must recheck slots after each sleep");
}

void test_busy_count_reaches_deadline_and_fails()
{
    FakeClock clock;
    unsigned polls = 0;
    const bool drained = voip::pjsip::detail::wait_for_zero_call_slots(
        25ms, [&] { ++polls; return 1U; },
        [&] { return clock.now(); }, [&](auto delay) { clock.sleep(delay); });
    require(!drained, "permanently busy slots must report timeout");
    require(clock.sleeps == 3 && polls == 4 && clock.time == TimePoint{30ms},
            "deadline must stop further polling once expired");
}

void test_busy_immediate_deadline_does_not_sleep()
{
    FakeClock clock;
    const bool drained = voip::pjsip::detail::wait_for_zero_call_slots(
        0ms, [] { return 1U; },
        [&] { return clock.now(); }, [&](auto delay) { clock.sleep(delay); });
    require(!drained && clock.sleeps == 0, "already expired busy wait must fail");
}

void test_zero_at_deadline_is_observed_as_drained()
{
    FakeClock clock;
    unsigned polls = 0;
    const bool drained = voip::pjsip::detail::wait_for_zero_call_slots(
        20ms, [&] { return (++polls == 3) ? 0U : 1U; },
        [&] { return clock.now(); }, [&](auto delay) { clock.sleep(delay); });
    require(drained, "zero count at deadline should succeed before time check");
    require(clock.sleeps == 2 && polls == 3, "must recheck count first");
}

void test_zero_count_does_not_prove_future_or_callback_quiescence()
{
    FakeClock clock;
    unsigned polls = 0;
    auto count = [&] { return (++polls == 2) ? 0U : 1U; };
    const bool drained = voip::pjsip::detail::wait_for_zero_call_slots(
        100ms, count, [&] { return clock.now(); },
        [&](auto delay) { clock.sleep(delay); });
    require(drained && polls == 2 && clock.sleeps == 1,
            "poll returns as soon as one observation is zero");
    require(count() != 0U,
            "a later count can be nonzero; zero is no quiescence barrier");
    // This is a synthetic count sequence, NOT a real SIP callback test.
}

} // namespace

int main()
{
    try {
        test_already_drained_has_no_delay();
        test_later_drain_succeeds_before_deadline();
        test_busy_count_reaches_deadline_and_fails();
        test_busy_immediate_deadline_does_not_sleep();
        test_zero_at_deadline_is_observed_as_drained();
        test_zero_count_does_not_prove_future_or_callback_quiescence();
        std::cout << "PASS: 6 deterministic PJSIP call-slot polling scenarios\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
