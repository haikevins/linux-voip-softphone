#include "threading/WorkerExecutor.hpp"

#include <stdexcept>
#include <utility>

namespace voip::threading {

WorkerExecutor::WorkerExecutor() : worker_([this]() { run(); }), worker_thread_id_(worker_.get_id())
{}

WorkerExecutor::~WorkerExecutor()
{
    shutdown();
}

std::future<void> WorkerExecutor::dispatch(Task task)
{
    if (!task) {
        throw std::invalid_argument("worker task must not be empty");
    }

    std::packaged_task<void()> packaged_task{std::move(task)};
    auto completion = packaged_task.get_future();

    {
        const std::lock_guard lock{mutex_};
        if (!accepting_) {
            throw std::runtime_error("worker executor is shut down");
        }
        tasks_.push_back(std::move(packaged_task));
    }

    condition_.notify_one();
    return completion;
}

void WorkerExecutor::shutdown()
{
    {
        const std::lock_guard lock{mutex_};
        accepting_ = false;
    }

    condition_.notify_all();

    const std::lock_guard join_lock{join_mutex_};
    if (worker_.joinable() && std::this_thread::get_id() != worker_thread_id_) {
        worker_.join();
    }
}

bool WorkerExecutor::accepting() const
{
    const std::lock_guard lock{mutex_};
    return accepting_;
}

std::thread::id WorkerExecutor::worker_thread_id() const noexcept
{
    return worker_thread_id_;
}

void WorkerExecutor::run()
{
    while (true) {
        std::packaged_task<void()> task;

        {
            std::unique_lock lock{mutex_};
            condition_.wait(lock, [this]() { return !tasks_.empty() || !accepting_; });

            if (tasks_.empty()) {
                return;
            }

            task = std::move(tasks_.front());
            tasks_.pop_front();
        }

        task();
    }
}

} // namespace voip::threading
