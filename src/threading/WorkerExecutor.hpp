#pragma once

#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <thread>

namespace voip::threading {

class WorkerExecutor final
{
  public:
    using Task = std::function<void()>;

    WorkerExecutor();
    ~WorkerExecutor();

    WorkerExecutor(const WorkerExecutor &) = delete;
    WorkerExecutor &operator=(const WorkerExecutor &) = delete;
    WorkerExecutor(WorkerExecutor &&) = delete;
    WorkerExecutor &operator=(WorkerExecutor &&) = delete;

    [[nodiscard]] std::future<void> dispatch(Task task);
    void shutdown();

    [[nodiscard]] bool accepting() const;
    [[nodiscard]] std::thread::id worker_thread_id() const noexcept;

  private:
    void run();

    mutable std::mutex mutex_;
    std::mutex join_mutex_;
    std::condition_variable condition_;
    std::deque<std::packaged_task<void()>> tasks_;
    bool accepting_{true};
    std::thread worker_;
    std::thread::id worker_thread_id_;
};

} // namespace voip::threading
