#pragma once

#include <utility>

namespace voip::pjsip::detail {

// Execute an account shutdown operation and translate its result into a
// fail-closed boolean. The caller supplies the real PJSUA2 shutdown2(false)
// operation and retains ownership on failure. This helper does NOT prove
// callback ingress quiescence or authorize destroying Account/Endpoint.
//
// Deliberately accepts an operation instead of a PJSUA2 Account so fake
// failures can exercise the *same invocation boundary* without SIP access.
template <typename ApiError, typename Operation, typename OnApiError, typename OnOtherError>
[[nodiscard]] bool attempt_account_shutdown(Operation &&operation, OnApiError &&on_api_error,
                                            OnOtherError &&on_other_error) noexcept
{
    try {
        std::forward<Operation>(operation)();
        return true;
    } catch (const ApiError &error) {
        try {
            std::forward<OnApiError>(on_api_error)(error);
        } catch (...) {
            // Logging failures must not turn a failed shutdown into success.
        }
    } catch (...) {
        try {
            std::forward<OnOtherError>(on_other_error)();
        } catch (...) {
            // No exception may escape this fail-closed boundary.
        }
    }
    return false;
}

} // namespace voip::pjsip::detail
