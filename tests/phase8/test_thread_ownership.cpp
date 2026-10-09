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

void require(bool condition, std::string_view description) {
    if (!condition) {
        throw std::runtime_error(std::string(description));
    }
}

// An external producer never executes a submitted callback, even under
// concurrent posting. All callbacks must run on one stable owned thread.
void test_multi_producer_handoff_preserves_single_execution_owner() {
    constexpr std::size_t producers = 4;
    constexpr std::size_t per_producer = 250;
    constexpr std::size_t total = producers * per_producer;

    const auto owner_thread = std::this_thread::get_id();
    SerialTaskWorker worker;
    std::vector<std::thread::id> callback_threads(total);
    std::vector<std::thread::id> producer_threads(producers);
    std::atomic<std::size_t> rejected{0};

    {
        std::vector<std::jthread> writers;
        writers.reserve(producers);
        for (std::size_t producer = 0; producer < producers; ++producer) {
            writers.emplace_back([&, producer] {
                producer_threads[producer] = std::this_thread::get_id();
                for (std::size_t index = 0; index < per_producer; ++index) {
                    const auto task_id = producer * per_producer + index;
                    if (!worker.post([&, task_id] {
                            callback_threads[task_id] = std::this_thread::get_id();
                        })) {
                        rejected.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            });
        }
        // Destruction of writers joins all producers before queue shutdown.
    }

    worker.shutdown();
    require(rejected.load() == 0, "all producers must post successfully");
    require(worker.completed() == total, "exactly one execution per accepted task");
    require(worker.failures() == 0, "no callbacks should fail");

    const auto worker_thread = callback_threads[0];
    require(worker_thread != owner_thread, "worker thread differs from owner");
    for (const auto producer_id : producer_threads) {
        require(worker_thread != producer_id, "callback must not run on producer");
    }
    for (const auto callback_id : callback_threads) {
        require(callback_id == worker_thread, "callback affinity must remain stable");
    }
}

// A task that posts another task cannot execute it reentrantly on the
// posting call stack. The nested task must be queued behind prior work.
void test_nested_handoff_remains_serial_and_fifo() {
    SerialTaskWorker worker;
    std::promise<void> entered;
    auto started = entered.get_future();
    std::promise<void> release;
    auto go = release.get_future().share();
    std::atomic<bool> nested_accepted{false};
    std::promise<void> nested_posted;
    auto nested_posted_future = nested_posted.get_future();
    std::vector<int> sequence; // Only accessed by worker until shutdown joins.
    std::vector<std::thread::id> observed_threads;

    require(worker.post([&] {
        entered.set_value();
        go.wait();
        sequence.push_back(1);
        observed_threads.push_back(std::this_thread::get_id());
        nested_accepted.store(worker.post([&] {
            sequence.push_back(3);
            observed_threads.push_back(std::this_thread::get_id());
        }));
        nested_posted.set_value();
    }), "post first blocking task");

    if (started.wait_for(5s) != std::future_status::ready) {
        release.set_value();
        worker.shutdown();
        require(false, "first callback did not start");
    }
    const bool second_accepted = worker.post([&] {
        sequence.push_back(2);
        observed_threads.push_back(std::this_thread::get_id());
    });
    release.set_value(); // Unblock before any possible assertion/destruction.
    const bool nested_post_finished =
        nested_posted_future.wait_for(5s) == std::future_status::ready;
    worker.shutdown();

    require(nested_post_finished, "nested post did not complete");
    require(second_accepted, "second task accepted");
    require(nested_accepted.load(), "nested post accepted");
    require(sequence == std::vector<int>({1, 2, 3}), "nested post respects FIFO");
    require(observed_threads.size() == 3, "three callbacks executed");
    for (const auto id : observed_threads) {
        require(id == observed_threads[0], "nested work must stay on same thread");
    }
    require(worker.completed() == 3 && worker.failures() == 0,
            "nested handoff completion accounting");
}

} // namespace

int main() {
    try {
        test_multi_producer_handoff_preserves_single_execution_owner();
        test_nested_handoff_remains_serial_and_fifo();
        std::cout << "PASS: 2 Phase 8 worker thread-ownership scenarios\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
