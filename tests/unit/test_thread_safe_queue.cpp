#include "concurrency/ThreadSafeQueue.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

using voip::concurrency::ThreadSafeQueue;

void require(bool condition, std::string_view description)
{
    if (!condition) {
        throw std::runtime_error(std::string(description));
    }
}

void test_fifo_and_empty()
{
    ThreadSafeQueue<int> queue;
    require(!queue.try_pop().has_value(), "empty queue must not produce an item");
    require(queue.push(1) && queue.push(2) && queue.push(3), "push failed");
    require(queue.size() == 3, "size after pushes");
    require(queue.try_pop() == 1, "FIFO item 1");
    require(queue.wait_pop() == 2, "FIFO item 2");
    require(queue.try_pop() == 3, "FIFO item 3");
    require(!queue.try_pop(), "queue should be empty");
}

void test_close_drains_and_rejects()
{
    ThreadSafeQueue<int> queue;
    require(queue.push(9) && queue.push(10), "pre-close pushes failed");
    queue.close();
    queue.close();
    require(queue.is_closed(), "close flag");
    require(!queue.push(11), "push after close must be rejected");
    require(queue.wait_pop() == 9, "drain first item");
    require(queue.wait_pop() == 10, "drain second item");
    require(!queue.wait_pop(), "closed and empty must return nullopt");
    require(!queue.try_pop(), "try_pop on closed and empty queue");
}

void test_wait_pop_wakes_on_push()
{
    ThreadSafeQueue<int> queue;
    std::promise<std::optional<int>> outcome;
    std::promise<void> started;
    auto result = outcome.get_future();
    auto started_result = started.get_future();
    std::jthread worker([&] {
        started.set_value();
        outcome.set_value(queue.wait_pop());
    });
    started_result.wait();
    const auto before_push = result.wait_for(std::chrono::milliseconds(20));
    const bool accepted = queue.push(42);
    const auto after_push = result.wait_for(std::chrono::seconds(3));
    queue.close(); // Also ensures the worker can exit if an assertion fails.
    require(before_push == std::future_status::timeout,
            "wait_pop should block when open and empty");
    require(accepted, "push to waiting consumer");
    require(after_push == std::future_status::ready, "waiting consumer did not wake on push");
    require(result.get() == 42, "woken consumer got wrong item");
}

void test_wait_pop_wakes_on_close()
{
    ThreadSafeQueue<int> queue;
    std::promise<std::optional<int>> outcome;
    std::promise<void> started;
    auto result = outcome.get_future();
    auto started_result = started.get_future();
    std::jthread worker([&] {
        started.set_value();
        outcome.set_value(queue.wait_pop());
    });
    started_result.wait();
    const auto before_close = result.wait_for(std::chrono::milliseconds(20));
    queue.close();
    const auto after_close = result.wait_for(std::chrono::seconds(3));
    require(before_close == std::future_status::timeout, "wait_pop should block before close");
    require(after_close == std::future_status::ready, "waiting consumer did not wake on close");
    require(!result.get(), "closed empty queue must return nullopt");
}

void test_move_only_items()
{
    ThreadSafeQueue<std::unique_ptr<int>> queue;
    require(queue.push(std::make_unique<int>(7)), "move-only push");
    auto item = queue.wait_pop();
    require(item && *item && **item == 7, "move-only pop");
    queue.close();
}

void test_pre_requested_stop_preserves_pending_item()
{
    ThreadSafeQueue<int> queue;
    require(queue.push(17), "push before cancellation");
    std::stop_source stop;
    require(stop.request_stop(), "request_stop should succeed");
    require(!queue.wait_pop(stop.get_token()), "already-cancelled wait must not consume an item");
    require(queue.try_pop() == 17, "cancelled wait must preserve queued item");
    require(!queue.is_closed(), "request_stop must not close the queue");
    require(queue.push(18), "queue remains usable after cancellation");
    require(queue.wait_pop() == 18, "uncancelled pop after cancellation");
    queue.close();
}

void test_stop_wakes_blocked_waiter_without_closing_queue()
{
    ThreadSafeQueue<int> queue;
    std::promise<void> started;
    std::promise<std::optional<int>> outcome;
    auto started_result = started.get_future();
    auto result = outcome.get_future();

    std::jthread consumer([&](const std::stop_token &stop) {
        started.set_value();
        outcome.set_value(queue.wait_pop(stop));
    });
    started_result.wait();
    const auto before_stop = result.wait_for(std::chrono::milliseconds(20));
    consumer.request_stop();
    const auto after_stop = result.wait_for(std::chrono::seconds(3));
    // The queue will also wake the waiter if a regression drops stop support,
    // so the test can fail without hanging a jthread destructor indefinitely.
    queue.close();
    consumer.join();

    require(before_stop == std::future_status::timeout,
            "empty cancellable wait must block before stop");
    require(after_stop == std::future_status::ready,
            "request_stop did not wake the blocked consumer");
    require(!result.get(), "stopped waiter must return nullopt");
}

void test_stopping_one_worker_keeps_queue_open_for_others()
{
    ThreadSafeQueue<int> queue;
    std::promise<std::optional<int>> first_result;
    auto first = first_result.get_future();
    std::jthread first_worker(
        [&](const std::stop_token &stop) { first_result.set_value(queue.wait_pop(stop)); });
    first_worker.request_stop();
    // A stopped worker must leave the queue usable by a different worker.
    // Close is a fallback wake-up if stop propagation breaks.
    const auto first_status = first.wait_for(std::chrono::seconds(3));
    if (first_status != std::future_status::ready) {
        queue.close();
    }
    first_worker.join();
    require(first_status == std::future_status::ready,
            "first worker must exit on local stop without queue close");
    require(!first.get(), "stopped worker must not receive work");
    require(!queue.is_closed(), "local stop must not close shared queue");

    std::promise<std::optional<int>> second_result;
    auto second = second_result.get_future();
    std::jthread second_worker(
        [&](const std::stop_token &stop) { second_result.set_value(queue.wait_pop(stop)); });
    const bool accepted = queue.push(23);
    const auto second_status = second.wait_for(std::chrono::seconds(3));
    queue.close();
    second_worker.join();
    require(accepted, "new work must be accepted after local stop");
    require(second_status == std::future_status::ready, "second worker did not receive work");
    require(second.get() == 23, "second worker must receive the queued item");
}

void test_stoppable_wait_drains_after_close()
{
    ThreadSafeQueue<int> queue;
    std::stop_source source;
    require(queue.push(31), "queue item before close");
    require(queue.push(32), "second queue item before close");
    queue.close();
    require(queue.wait_pop(source.get_token()) == 31,
            "stop-aware pop must drain first item after close");
    require(queue.wait_pop(source.get_token()) == 32,
            "stop-aware pop must drain second item after close");
    require(!queue.wait_pop(source.get_token()), "closed and drained stop-aware pop must end");
}

void test_close_wakes_multiple_stoppable_workers()
{
    constexpr int worker_count = 12;
    ThreadSafeQueue<int> queue;
    std::atomic<int> finished = 0;
    std::vector<std::jthread> workers;
    workers.reserve(worker_count);
    for (int i = 0; i < worker_count; ++i) {
        workers.emplace_back([&](const std::stop_token &stop) {
            if (!queue.wait_pop(stop)) {
                ++finished;
            }
        });
    }
    queue.close();
    for (auto &worker : workers) {
        worker.join();
    }
    require(finished == worker_count, "close must wake and terminate all stoppable waiters");
    require(!queue.wait_pop(), "closed and drained queue remains exhausted");
}

void test_multi_producer_multi_consumer_integrity()
{
    constexpr int producers = 4;
    constexpr int consumers = 4;
    constexpr int items_per_producer = 1000;
    constexpr int total_items = producers * items_per_producer;

    ThreadSafeQueue<int> queue;
    std::vector<std::atomic<int>> seen(total_items);
    for (auto &counter : seen) {
        counter.store(0);
    }
    std::atomic<int> received = 0;
    std::atomic<int> rejected = 0;
    std::atomic<int> out_of_range = 0;

    std::vector<std::jthread> workers;
    workers.reserve(producers + consumers);
    for (int consumer = 0; consumer < consumers; ++consumer) {
        workers.emplace_back([&] {
            while (auto value = queue.wait_pop()) {
                if (*value < 0 || *value >= total_items) {
                    ++out_of_range;
                    continue;
                }
                ++seen[static_cast<std::size_t>(*value)];
                ++received;
            }
        });
    }

    std::vector<std::jthread> writers;
    writers.reserve(producers);
    for (int producer = 0; producer < producers; ++producer) {
        writers.emplace_back([&, producer] {
            for (int index = 0; index < items_per_producer; ++index) {
                if (!queue.push(producer * items_per_producer + index)) {
                    ++rejected;
                }
            }
        });
    }

    // All producers finish before close(); the queue is then drained by
    // consumers. This avoids using sleeps as synchronization.
    writers.clear();
    queue.close();
    workers.clear();

    require(rejected == 0, "unexpected rejected producer item");
    require(out_of_range == 0, "out-of-range item");
    require(received == total_items, "lost item in MPMC run");
    for (const auto &count : seen) {
        require(count == 1, "duplicate or missing item in MPMC run");
    }
}

} // namespace

int main()
{
    try {
        test_fifo_and_empty();
        test_close_drains_and_rejects();
        test_wait_pop_wakes_on_push();
        test_wait_pop_wakes_on_close();
        test_move_only_items();
        test_pre_requested_stop_preserves_pending_item();
        test_stop_wakes_blocked_waiter_without_closing_queue();
        test_stopping_one_worker_keeps_queue_open_for_others();
        test_stoppable_wait_drains_after_close();
        test_close_wakes_multiple_stoppable_workers();
        test_multi_producer_multi_consumer_integrity();
        std::cout << "PASS: 11 Phase 8 ThreadSafeQueue scenarios\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
