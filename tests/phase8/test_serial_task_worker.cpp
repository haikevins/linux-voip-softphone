#include "concurrency/SerialTaskWorker.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <future>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

using voip::concurrency::SerialTaskWorker;
using namespace std::chrono_literals;

void require(bool condition, std::string_view description)
{
    if (!condition) {
        throw std::runtime_error(std::string(description));
    }
}

void test_fifo_single_worker_thread()
{
    SerialTaskWorker worker;
    std::vector<int> order;
    std::vector<std::thread::id> observed_threads;
    const auto posting_thread = std::this_thread::get_id();
    for (int i = 0; i < 100; ++i) {
        require(worker.post([&, i] {
            order.push_back(i);
            observed_threads.push_back(std::this_thread::get_id());
        }),
                "post FIFO work");
    }
    worker.shutdown();
    require(order.size() == 100, "all FIFO work executed");
    for (int i = 0; i < 100; ++i) {
        require(order[static_cast<std::size_t>(i)] == i, "FIFO ordering");
        require(observed_threads[static_cast<std::size_t>(i)] != posting_thread,
                "work executes on a different thread");
        require(observed_threads[static_cast<std::size_t>(i)] == observed_threads[0],
                "every task executes on one owned worker thread");
    }
    require(worker.completed() == 100 && worker.failures() == 0, "successful task counters");
}

void test_shutdown_drains_rejects_and_is_idempotent()
{
    SerialTaskWorker worker;
    require(!worker.post({}), "empty callable rejected even before close");
    std::atomic<int> received{0};
    for (int i = 0; i < 32; ++i) {
        require(worker.post([&] { ++received; }), "post before shutdown");
    }
    worker.shutdown();
    worker.shutdown();
    require(received == 32, "shutdown drained accepted work");
    require(worker.completed() == 32, "completed count after shutdown");
    require(!worker.post([] {}), "post after shutdown rejected");
    require(!worker.post({}), "empty task rejected");
}

void test_exception_containment_and_accounting()
{
    SerialTaskWorker worker;
    std::atomic<int> reached{0};
    require(worker.post([] { throw std::runtime_error("intentional test error"); }),
            "post throwing task");
    require(worker.post([&] { ++reached; }), "post following task");
    worker.shutdown();
    require(reached == 1, "worker continues after exception");
    require(worker.completed() == 2, "failed task counts as completed");
    require(worker.failures() == 1, "exception counted without terminating");
}

void test_request_close_from_callback()
{
    SerialTaskWorker worker;
    std::atomic<bool> later_post_rejected{false};
    std::promise<void> callback_closed;
    auto closed = callback_closed.get_future();
    require(worker.post([&] {
        worker.request_close();
        later_post_rejected = !worker.post([] {});
        callback_closed.set_value();
    }),
            "post self-closing task");
    const bool closed_by_callback = closed.wait_for(3s) == std::future_status::ready;
    worker.shutdown();
    require(closed_by_callback, "callback did not close the queue");
    require(later_post_rejected, "close inside callback rejects new tasks");
    require(worker.completed() == 1, "self-closing task completed");
}

void test_shutdown_from_callback_is_refused_without_deadlock()
{
    SerialTaskWorker worker;
    std::atomic<bool> refused{false};
    require(worker.post([&] {
        try {
            worker.shutdown();
        } catch (const std::logic_error &) {
            refused = true;
        }
    }),
            "post self-shutdown attempt");
    worker.shutdown();
    require(refused, "self-shutdown explicitly refused");
    require(worker.failures() == 0, "handled rejection not a failed task");
}

void test_shutdown_waits_for_running_task()
{
    SerialTaskWorker worker;
    std::promise<void> entered;
    auto entered_future = entered.get_future();
    std::promise<void> release;
    auto release_future = release.get_future().share();
    std::atomic<int> reached{0};
    require(worker.post([&] {
        entered.set_value();
        release_future.wait();
        ++reached;
    }),
            "post blocked task");
    const bool started = entered_future.wait_for(3s) == std::future_status::ready;
    if (!started) {
        // Release in all error paths to keep the worker destructor joinable.
        release.set_value();
        worker.shutdown();
        require(false, "worker never started blocked task");
    }
    require(worker.post([&] { ++reached; }), "queue task while callback blocked");
    worker.request_close();
    auto joined = std::async(std::launch::async, [&] { worker.shutdown(); });
    const bool still_waiting = joined.wait_for(30ms) == std::future_status::timeout;
    release.set_value(); // Always release before assertions/destruction.
    require(joined.wait_for(3s) == std::future_status::ready,
            "shutdown did not finish after running task released");
    joined.get();
    require(still_waiting, "shutdown must wait for in-flight callback");
    require(reached == 2, "shutdown drains pending work after in-flight task");
}

void test_destructor_drains_outstanding_work()
{
    std::atomic<int> reached{0};
    {
        SerialTaskWorker worker;
        for (int i = 0; i < 300; ++i) {
            require(worker.post([&] { ++reached; }), "post before destruction");
        }
    }
    require(reached == 300, "destructor joins and drains accepted tasks");
}

void test_concurrent_shutdown_calls()
{
    SerialTaskWorker worker;
    std::atomic<int> reached{0};
    for (int i = 0; i < 200; ++i) {
        require(worker.post([&] { ++reached; }), "post before concurrent shutdown");
    }
    std::jthread closer1([&] { worker.shutdown(); });
    std::jthread closer2([&] { worker.shutdown(); });
    closer1.join();
    closer2.join();
    require(reached == 200, "concurrent external shutdown drains once");
    require(worker.completed() == 200, "shutdown completion count");
}

void test_concurrent_producers_no_lost_tasks()
{
    constexpr int producers = 4;
    constexpr int per_producer = 500;
    constexpr int total = producers * per_producer;
    SerialTaskWorker worker;
    std::vector<int> seen(total, 0); // Only accessed on worker, then after join.
    std::atomic<int> rejected{0};
    std::vector<std::jthread> writers;
    for (int producer = 0; producer < producers; ++producer) {
        writers.emplace_back([&, producer] {
            for (int index = 0; index < per_producer; ++index) {
                const int id = producer * per_producer + index;
                if (!worker.post([&, id] { ++seen[static_cast<std::size_t>(id)]; })) {
                    ++rejected;
                }
            }
        });
    }
    writers.clear();
    worker.shutdown();
    require(rejected == 0, "producer unexpectedly rejected before close");
    require(worker.completed() == total, "all posted tasks completed");
    for (int count : seen) {
        require(count == 1, "missing or duplicate task");
    }
}

void test_close_races_with_posts()
{
    SerialTaskWorker worker;
    constexpr int producers = 4;
    constexpr int per_producer = 500;
    std::atomic<int> accepted{0};
    std::atomic<int> rejected{0};
    std::atomic<int> reached{0};
    std::vector<std::jthread> writers;
    for (int producer = 0; producer < producers; ++producer) {
        writers.emplace_back([&] {
            for (int index = 0; index < per_producer; ++index) {
                if (worker.post([&] { ++reached; })) {
                    ++accepted;
                } else {
                    ++rejected;
                }
                std::this_thread::yield();
            }
        });
    }
    worker.request_close();
    writers.clear();
    worker.shutdown();
    require(accepted + rejected == producers * per_producer,
            "every post reports acceptance or rejection");
    require(reached == accepted, "exactly accepted tasks executed before join");
    require(worker.completed() == static_cast<std::uint64_t>(accepted.load()),
            "completed count equals accepted tasks");
}

} // namespace

int main()
{
    try {
        test_fifo_single_worker_thread();
        test_shutdown_drains_rejects_and_is_idempotent();
        test_exception_containment_and_accounting();
        test_request_close_from_callback();
        test_shutdown_from_callback_is_refused_without_deadlock();
        test_shutdown_waits_for_running_task();
        test_destructor_drains_outstanding_work();
        test_concurrent_shutdown_calls();
        test_concurrent_producers_no_lost_tasks();
        test_close_races_with_posts();
        std::cout << "PASS: 10 Phase 8 SerialTaskWorker scenarios\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
