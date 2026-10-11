#pragma once

#include "concurrency/ThreadSafeQueue.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>

namespace voip::concurrency {

// Owns one thread for FIFO, serial execution of callable tasks.
//
// Lifecycle:
// - post() accepts nonempty tasks until request_close()/shutdown() closes
//   the queue. Every accepted task runs exactly once before shutdown returns.
// - request_close() is nonblocking and may be called from a task.
// - shutdown() closes, drains, then joins; concurrent external shutdown calls
//   are serialized. Calling shutdown() on this worker's thread is rejected.
// - The destructor drains and joins. The owner must destroy this object from
//   outside its worker thread and synchronize destruction with all API users.
// - Task exceptions are contained so subsequent tasks still run; failures()
//   records the number of throwing tasks. The exception itself is not stored.
// - No Qt or PJSIP thread-safety/ownership guarantee is implied here.
class SerialTaskWorker final
{
  public:
    using Task = std::function<void()>;

    SerialTaskWorker() : worker_([this] { run(); }), worker_id_(worker_.get_id()) {}

    ~SerialTaskWorker()
    {
        // Destruction from a worker callback cannot be supported: doing so
        // would destroy the queue before run() has finished using it.
        shutdown();
    }

    SerialTaskWorker(const SerialTaskWorker &) = delete;
    SerialTaskWorker &operator=(const SerialTaskWorker &) = delete;
    SerialTaskWorker(SerialTaskWorker &&) = delete;
    SerialTaskWorker &operator=(SerialTaskWorker &&) = delete;

    [[nodiscard]] bool post(Task task)
    {
        if (!task) {
            return false;
        }
        return tasks_.push(std::move(task));
    }

    void request_close()
    {
        tasks_.close();
    }

    void shutdown()
    {
        if (std::this_thread::get_id() == worker_id_) {
            throw std::logic_error("SerialTaskWorker::shutdown cannot join its own worker thread");
        }
        request_close();
        // Protect std::jthread::join against concurrent external shutdowns.
        std::lock_guard lock(join_mutex_);
        if (worker_.joinable()) {
            worker_.join();
        }
    }

    [[nodiscard]] std::uint64_t completed() const noexcept
    {
        return completed_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] std::uint64_t failures() const noexcept
    {
        return failures_.load(std::memory_order_relaxed);
    }

  private:
    void run() noexcept
    {
        while (auto task = tasks_.wait_pop()) {
            try {
                (*task)();
            } catch (...) {
                failures_.fetch_add(1, std::memory_order_relaxed);
            }
            completed_.fetch_add(1, std::memory_order_relaxed);
        }
    }

    ThreadSafeQueue<Task> tasks_;
    std::atomic<std::uint64_t> completed_{0};
    std::atomic<std::uint64_t> failures_{0};
    std::jthread worker_;
    const std::thread::id worker_id_;
    std::mutex join_mutex_;
};

} // namespace voip::concurrency
