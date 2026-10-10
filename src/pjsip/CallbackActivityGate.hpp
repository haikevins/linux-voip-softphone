#pragma once

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <utility>

namespace voip::pjsip::detail {

// Tracks callback method entries. try_enter() preserves the original
// admission-only model used by synthetic tests. Production PJSUA2 callbacks
// use enter(), which also counts callbacks rejected after close() until the
// callback method returns. A closed and empty gate still does NOT stop an
// external callback source by itself; the PJSUA2 source must be quiesced by
// its own lifecycle contract before the callback target can be destroyed.
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

    class Entry final
    {
      public:
        Entry(const Entry &) = delete;
        Entry &operator=(const Entry &) = delete;

        Entry(Entry &&other) noexcept
            : owner_(std::exchange(other.owner_, nullptr)), admitted_(other.admitted_)
        {
            other.admitted_ = false;
        }

        Entry &operator=(Entry &&other) noexcept
        {
            if (this != &other) {
                release();
                owner_ = std::exchange(other.owner_, nullptr);
                admitted_ = other.admitted_;
                other.admitted_ = false;
            }
            return *this;
        }

        ~Entry()
        {
            release();
        }

        [[nodiscard]] bool admitted() const noexcept
        {
            return admitted_;
        }

        void release() noexcept
        {
            if (auto *owner = std::exchange(owner_, nullptr)) {
                owner->leave();
            }
            admitted_ = false;
        }

      private:
        friend class CallbackActivityGate;
        Entry(CallbackActivityGate *owner, const bool admitted) noexcept
            : owner_(owner), admitted_(admitted)
        {}

        CallbackActivityGate *owner_;
        bool admitted_;
    };

    [[nodiscard]] std::optional<Permit> try_enter()
    {
        const std::lock_guard lock{mutex_};
        if (closed_) {
            return std::nullopt;
        }
        ++active_;
        return Permit{this};
    }

    // Production PJSUA2 callback entry uses this form so even a callback
    // rejected after close() remains counted until the callback method exits.
    [[nodiscard]] Entry enter()
    {
        const std::lock_guard lock{mutex_};
        ++active_;
        return Entry{this, !closed_};
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

    // Reuse is only valid after the previous callback source is gone and all
    // tracked entries have returned.
    [[nodiscard]] bool reset_if_empty() noexcept
    {
        const std::lock_guard lock{mutex_};
        if (active_ != 0) {
            return false;
        }
        closed_ = false;
        return true;
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
