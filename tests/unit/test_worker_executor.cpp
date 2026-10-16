#include "threading/WorkerExecutor.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <future>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {

using voip::threading::WorkerExecutor;

} // namespace

TEST(WorkerExecutorTest, ExecutesTasksOnDedicatedWorkerThread)
{
    WorkerExecutor executor;
    const auto caller_thread = std::this_thread::get_id();

    std::promise<std::thread::id> observed_thread;
    auto observed_thread_future = observed_thread.get_future();
    auto completion = executor.dispatch(
        [&observed_thread]() { observed_thread.set_value(std::this_thread::get_id()); });

    completion.get();

    const auto worker_thread = observed_thread_future.get();
    EXPECT_EQ(worker_thread, executor.worker_thread_id());
    EXPECT_NE(worker_thread, caller_thread);
}

TEST(WorkerExecutorTest, PreservesDispatchOrder)
{
    WorkerExecutor executor;
    std::vector<int> observed_order;

    auto first = executor.dispatch([&observed_order]() { observed_order.push_back(1); });
    auto second = executor.dispatch([&observed_order]() { observed_order.push_back(2); });
    auto third = executor.dispatch([&observed_order]() { observed_order.push_back(3); });

    first.get();
    second.get();
    third.get();

    EXPECT_EQ(observed_order, (std::vector<int>{1, 2, 3}));
}

TEST(WorkerExecutorTest, CapturesTaskFailureAndContinuesProcessing)
{
    WorkerExecutor executor;
    std::atomic<int> completed_tasks{0};

    auto failing = executor.dispatch([]() { throw std::runtime_error("task failed"); });
    auto succeeding = executor.dispatch([&completed_tasks]() { ++completed_tasks; });

    EXPECT_THROW(failing.get(), std::runtime_error);
    EXPECT_NO_THROW(succeeding.get());
    EXPECT_EQ(completed_tasks.load(), 1);
}

TEST(WorkerExecutorTest, ShutdownDrainsQueuedTasksAndRejectsNewWork)
{
    WorkerExecutor executor;
    std::atomic<int> completed_tasks{0};

    std::promise<void> first_started;
    auto first_started_future = first_started.get_future();

    std::promise<void> release_first;
    auto release_first_future = release_first.get_future().share();

    auto first = executor.dispatch([&]() {
        first_started.set_value();
        release_first_future.wait();
        ++completed_tasks;
    });
    auto second = executor.dispatch([&completed_tasks]() { ++completed_tasks; });

    first_started_future.wait();

    std::thread shutdown_thread{[&executor]() { executor.shutdown(); }};
    release_first.set_value();
    shutdown_thread.join();

    EXPECT_NO_THROW(first.get());
    EXPECT_NO_THROW(second.get());
    EXPECT_EQ(completed_tasks.load(), 2);
    EXPECT_FALSE(executor.accepting());
    EXPECT_THROW(static_cast<void>(executor.dispatch([]() {})), std::runtime_error);
}

TEST(WorkerExecutorTest, RejectsEmptyTasks)
{
    WorkerExecutor executor;
    EXPECT_THROW(static_cast<void>(executor.dispatch(WorkerExecutor::Task{})),
                 std::invalid_argument);
}
