#pragma once

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <queue>
#include <utility>

namespace voip::threading {

template <typename T> class ThreadSafeQueue final
{
  public:
    ThreadSafeQueue() = default;
    ~ThreadSafeQueue() = default;

    ThreadSafeQueue(const ThreadSafeQueue &) = delete;
    ThreadSafeQueue &operator=(const ThreadSafeQueue &) = delete;
    ThreadSafeQueue(ThreadSafeQueue &&) = delete;
    ThreadSafeQueue &operator=(ThreadSafeQueue &&) = delete;

    [[nodiscard]] bool push(T value)
    {
        {
            const std::lock_guard lock{mutex_};
            if (closed_) {
                return false;
            }
            queue_.push(std::move(value));
        }

        condition_.notify_one();
        return true;
    }

    [[nodiscard]] std::optional<T> wait_and_pop()
    {
        std::unique_lock lock{mutex_};
        condition_.wait(lock, [this]() { return closed_ || !queue_.empty(); });

        if (queue_.empty()) {
            return std::nullopt;
        }

        auto value = std::move(queue_.front());
        queue_.pop();
        return value;
    }

    void close()
    {
        {
            const std::lock_guard lock{mutex_};
            closed_ = true;
        }
        condition_.notify_all();
    }

    [[nodiscard]] bool closed() const
    {
        const std::lock_guard lock{mutex_};
        return closed_;
    }

    [[nodiscard]] std::size_t size() const
    {
        const std::lock_guard lock{mutex_};
        return queue_.size();
    }

  private:
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::queue<T> queue_;
    bool closed_{false};
};

} // namespace voip::threading
