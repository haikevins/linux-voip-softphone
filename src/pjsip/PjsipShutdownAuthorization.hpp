#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <optional>

namespace voip::pjsip::detail {

// These are four distinct *receipts*, not four observations supplied by this
// helper. In particular, LocalCallbacksDrained MUST NOT be presented as
// ExternalCallbackIngressQuiescent. PJSUA2 does not supply the latter here.
enum class ShutdownProof : std::uint8_t
{
    CallSlotsDrained,
    AccountShutdownSucceeded,
    LocalCallbacksDrained,
    ExternalCallbackIngressQuiescent,
    Count
};

enum class ShutdownReceipt : std::uint8_t
{
    Verified,
    Timeout,
    Busy,
    Exception,
    NotQuiescent
};

enum class ShutdownAuthorizationState : std::uint8_t
{
    Idle,
    Collecting,
    Denied,
    Authorized
};

// Pure bookkeeping; does not own, destroy, or fence any PJSUA2 object.
// The caller is responsible for supplying genuine per-attempt evidence.
// Nothing here establishes an external callback-quiescence guarantee.
class PjsipShutdownAuthorization final
{
  public:
    using Generation = std::uint64_t;

    // Every attempt discards earlier receipts, even if an older attempt has
    // not finished. A successful authorization cannot be restarted.
    [[nodiscard]] std::optional<Generation> begin_attempt()
    {
        const std::lock_guard lock{mutex_};
        if (state_ == ShutdownAuthorizationState::Authorized ||
            generation_ == std::numeric_limits<Generation>::max()) {
            return std::nullopt;
        }
        ++generation_;
        verified_.fill(false);
        failure_.reset();
        state_ = ShutdownAuthorizationState::Collecting;
        return generation_;
    }

    [[nodiscard]] bool report(const Generation generation, const ShutdownProof proof,
                              const ShutdownReceipt receipt)
    {
        const std::lock_guard lock{mutex_};
        if (generation != generation_ || state_ != ShutdownAuthorizationState::Collecting) {
            return false;
        }
        const auto index = static_cast<std::size_t>(proof);
        if (index >= verified_.size() || verified_[index]) {
            return false;
        }
        if (receipt != ShutdownReceipt::Verified) {
            failure_ = receipt;
            state_ = ShutdownAuthorizationState::Denied;
            return true;
        }
        verified_[index] = true;
        return true;
    }

    // True means only that the four caller-supplied receipts were verified in
    // the current generation. It does not trigger production object release.
    [[nodiscard]] bool authorize(const Generation generation)
    {
        const std::lock_guard lock{mutex_};
        if (generation != generation_ || state_ != ShutdownAuthorizationState::Collecting ||
            !std::all_of(verified_.begin(), verified_.end(), [](const bool ok) { return ok; })) {
            return false;
        }
        state_ = ShutdownAuthorizationState::Authorized;
        return true;
    }

    [[nodiscard]] ShutdownAuthorizationState state() const
    {
        const std::lock_guard lock{mutex_};
        return state_;
    }

    [[nodiscard]] std::optional<ShutdownReceipt> failure() const
    {
        const std::lock_guard lock{mutex_};
        return failure_;
    }

  private:
    mutable std::mutex mutex_;
    Generation generation_{0};
    std::array<bool, static_cast<std::size_t>(ShutdownProof::Count)> verified_{};
    ShutdownAuthorizationState state_{ShutdownAuthorizationState::Idle};
    std::optional<ShutdownReceipt> failure_;
};

} // namespace voip::pjsip::detail
