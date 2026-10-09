#pragma once

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <utility>

namespace voip::pjsip::detail {

// Tracks callbacks that successfully ENTERED this gate. A closed and empty
// gate only proves that these admitted callbacks have returned. It does NOT
// prevent an external PJSUA2 callback from entering after the containing
// owner is destroyed; the library callback source must be stopped separately.
class CallbackActivityGate final
{
  public:
    class Permit final
    {
      public:
        Permit(const Permit &) = delete;
        Permit &operator=(const Permit &) = delete;

        Permit(Permit &&other) noexcept : owner_(std::exchange(other.owner_, nullptr)) {}

        Permit &operator=(Permit &&other) noexcept
        {
            if (this != &other) {
                release();
                owner_ = std::exchange(other.owner_, nullptr);
            }
            return *this;
        }

        ~Permit()
        {
            release();
        }

        void release() noexcept
        {
            if (auto *owner = std::exchange(owner_, nullptr)) {
                owner->leave();
            }
        }

      private:
        friend class CallbackActivityGate;
        explicit Permit(CallbackActivityGate *owner) noexcept : owner_(owner) {}
        CallbackActivityGate *owner_;
    };

    CallbackActivityGate() = default;
    CallbackActivityGate(const CallbackActivityGate &) = delete;
    CallbackActivityGate &operator=(const CallbackActivityGate &) = delete;
    CallbackActivityGate(CallbackActivityGate &&) = delete;
    CallbackActivityGate &operator=(CallbackActivityGate &&) = delete;

    [[nodiscard]] std::optional<Permit> try_enter()
    {
        const std::lock_guard lock{mutex_};
        if (closed_) {
            return std::nullopt;
        }
        ++active_;
        return Permit{this};
    }

    void close() noexcept
    {
        const std::lock_guard lock{mutex_};
        closed_ = true;
        condition_.notify_all();
    }

    // A timeout returning false MUST NOT authorize destruction. Even true
    // requires an external guarantee of no future PJSUA2 callback entries.
    [[nodiscard]] bool wait_for_empty_for(const std::chrono::milliseconds timeout)
    {
        std::unique_lock lock{mutex_};
        if (!closed_ || timeout < std::chrono::milliseconds::zero()) {
            return false;
        }
        return condition_.wait_for(lock, timeout, [this] { return active_ == 0; });
    }

    [[nodiscard]] bool closed_and_empty() const
    {
        const std::lock_guard lock{mutex_};
        return closed_ && active_ == 0;
    }

    [[nodiscard]] std::size_t admitted_callbacks() const
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
        if (active_ == 0) {
            condition_.notify_all();
        }
    }

    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::size_t active_{0};
    bool closed_{false};
};

} // namespace voip::pjsip::detail
