#pragma once

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <queue>
#include <stop_token>
#include <utility>

namespace voip::concurrency {

// Unbounded, FIFO, multi-producer/multi-consumer queue.
//
// Contract:
// - push() returns false without enqueuing once close() has completed.
// - close() is idempotent and wakes all blocked consumers.
// - Consumers may drain queued items after close().
// - wait_pop() returns nullopt only when closed AND drained.
// - wait_pop(stop_token) also returns nullopt on cancellation, leaving pending
//   items untouched when cancellation is observed before the dequeue.
// - A cancelled waiter does not close the queue or stop other consumers.
// - The queue must outlive every producer/consumer calling into it.
// - Bounded capacity and worker ownership remain out of scope.
template <typename T>
class ThreadSafeQueue final {
public:
    ThreadSafeQueue() = default;
    ~ThreadSafeQueue() = default;

    ThreadSafeQueue(const ThreadSafeQueue&) = delete;
    ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;
    ThreadSafeQueue(ThreadSafeQueue&&) = delete;
    ThreadSafeQueue& operator=(ThreadSafeQueue&&) = delete;

    [[nodiscard]] bool push(T item) {
        {
            std::lock_guard lock(mutex_);
            if (closed_) {
                return false;
            }
            queue_.push(std::move(item));
        }
        available_.notify_one();
        return true;
    }

    [[nodiscard]] std::optional<T> try_pop() {
        std::lock_guard lock(mutex_);
        return pop_locked();
    }

    [[nodiscard]] std::optional<T> wait_pop() {
        std::unique_lock lock(mutex_);
        available_.wait(lock, [this] {
            return closed_ || !queue_.empty();
        });
        return pop_locked();
    }

    // Cancellation is local to this wait, not a global queue shutdown.
    // If stop is observed before dequeue (even with pending data), no item
    // is consumed. A concurrent stop/dequeue race may resolve either way.
    // The owning thread must still join before destroying this queue.
    [[nodiscard]] std::optional<T> wait_pop(std::stop_token stop) {
        std::unique_lock lock(mutex_);
        const bool ready = available_.wait(lock, stop, [this] {
            return closed_ || !queue_.empty();
        });
        if (!ready || stop.stop_requested()) {
            return std::nullopt;
        }
        return pop_locked();
    }

    void close() {
        {
            std::lock_guard lock(mutex_);
            closed_ = true;
        }
        available_.notify_all();
    }

    [[nodiscard]] bool is_closed() const {
        std::lock_guard lock(mutex_);
        return closed_;
    }

    [[nodiscard]] std::size_t size() const {
        std::lock_guard lock(mutex_);
        return queue_.size();
    }

private:
    // Caller must hold mutex_. If moving T throws, the item is not removed.
    [[nodiscard]] std::optional<T> pop_locked() {
        if (queue_.empty()) {
            return std::nullopt;
        }
        std::optional<T> item(std::in_place, std::move(queue_.front()));
        queue_.pop();
        return item;
    }

    mutable std::mutex mutex_;
    // C++20 stop_token-aware waits require condition_variable_any.
    std::condition_variable_any available_;
    std::queue<T> queue_;
    bool closed_ = false;
};

}  // namespace voip::concurrency
