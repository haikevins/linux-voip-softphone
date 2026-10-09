#pragma once

#include <chrono>

namespace voip::pjsip::detail {

// Polling policy only. Production supplies pjsua_call_get_count(), steady
// clock, and sleep. The injected operations make all deadline branches
// reproducible without SIP credentials or wall-clock sleeps.
// A true return means only that a count probe observed zero at that instant;
// it is NOT a callback-quiescence or thread-lifetime guarantee.
template <typename Count, typename Now, typename Sleep>
[[nodiscard]] bool wait_for_zero_call_slots(
    const std::chrono::milliseconds timeout, Count &&count, Now &&now,
    Sleep &&sleep)
{
    const auto deadline = now() + timeout;
    while (count() != 0U) {
        if (now() >= deadline) {
            return false;
        }
        sleep(std::chrono::milliseconds{10});
    }
    return true;
}

} // namespace voip::pjsip::detail
