#include "concurrency/ThreadSafeQueue.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

using voip::concurrency::ThreadSafeQueue;

void require(bool condition, std::string_view description) {
    if (!condition) {
        throw std::runtime_error(std::string(description));
    }
}

void test_fifo_and_empty() {
    ThreadSafeQueue<int> queue;
    require(!queue.try_pop().has_value(), "empty queue must not produce an item");
    require(queue.push(1) && queue.push(2) && queue.push(3), "push failed");
    require(queue.size() == 3, "size after pushes");
    require(queue.try_pop() == 1, "FIFO item 1");
    require(queue.wait_pop() == 2, "FIFO item 2");
    require(queue.try_pop() == 3, "FIFO item 3");
    require(!queue.try_pop(), "queue should be empty");
}

void test_close_drains_and_rejects() {
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

void test_wait_pop_wakes_on_push() {
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
    queue.close();  // Also ensures the worker can exit if an assertion fails.
    require(before_push == std::future_status::timeout,
            "wait_pop should block when open and empty");
    require(accepted, "push to waiting consumer");
    require(after_push == std::future_status::ready,
            "waiting consumer did not wake on push");
    require(result.get() == 42, "woken consumer got wrong item");
}

void test_wait_pop_wakes_on_close() {
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
    require(before_close == std::future_status::timeout,
            "wait_pop should block before close");
    require(after_close == std::future_status::ready,
            "waiting consumer did not wake on close");
    require(!result.get(), "closed empty queue must return nullopt");
}

void test_move_only_items() {
    ThreadSafeQueue<std::unique_ptr<int>> queue;
    require(queue.push(std::make_unique<int>(7)), "move-only push");
    auto item = queue.wait_pop();
    require(item && *item && **item == 7, "move-only pop");
    queue.close();
}

void test_multi_producer_multi_consumer_integrity() {
    constexpr int producers = 4;
    constexpr int consumers = 4;
    constexpr int items_per_producer = 1000;
    constexpr int total_items = producers * items_per_producer;

    ThreadSafeQueue<int> queue;
    std::vector<std::atomic<int>> seen(total_items);
    for (auto& counter : seen) {
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
    for (const auto& count : seen) {
        require(count == 1, "duplicate or missing item in MPMC run");
    }
}

}  // namespace

int main() {
    try {
        test_fifo_and_empty();
        test_close_drains_and_rejects();
        test_wait_pop_wakes_on_push();
        test_wait_pop_wakes_on_close();
        test_move_only_items();
        test_multi_producer_multi_consumer_integrity();
        std::cout << "PASS: 6 Phase 8 ThreadSafeQueue scenarios\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
