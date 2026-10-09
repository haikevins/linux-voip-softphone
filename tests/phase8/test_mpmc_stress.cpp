#include "concurrency/SerialTaskWorker.hpp"
#include "concurrency/ThreadSafeQueue.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <latch>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

using voip::concurrency::SerialTaskWorker;
using voip::concurrency::ThreadSafeQueue;

void require(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

struct Event {
    std::size_t producer;
    std::size_t sequence;
};

// High-contention delivery: unique IDs are stored in atomic slots so that
// detecting a duplicate is itself data-race-free even if the queue is broken.
void test_100000_event_mpmc_integrity() {
    constexpr std::size_t producers = 4;
    constexpr std::size_t consumers = 4;
    constexpr std::size_t per_producer = 25'000;
    constexpr std::size_t total = producers * per_producer;

    ThreadSafeQueue<Event> queue;
    std::vector<std::atomic<unsigned>> seen(total);
    for (auto& value : seen) {
        value.store(0, std::memory_order_relaxed);
    }
    std::atomic<std::size_t> accepted{0};
    std::atomic<std::size_t> rejected{0};
    std::atomic<std::size_t> received{0};
    std::atomic<std::size_t> malformed{0};
    std::latch begin(1);

    std::vector<std::jthread> readers;
    readers.reserve(consumers);
    for (std::size_t i = 0; i < consumers; ++i) {
        readers.emplace_back([&] {
            begin.wait();
            while (auto event = queue.wait_pop()) {
                if (event->producer >= producers ||
                    event->sequence >= per_producer) {
                    malformed.fetch_add(1, std::memory_order_relaxed);
                    continue;
                }
                const std::size_t id =
                    event->producer * per_producer + event->sequence;
                seen[id].fetch_add(1, std::memory_order_relaxed);
                received.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    std::vector<std::jthread> writers;
    writers.reserve(producers);
    for (std::size_t producer = 0; producer < producers; ++producer) {
        writers.emplace_back([&, producer] {
            begin.wait();
            for (std::size_t sequence = 0; sequence < per_producer; ++sequence) {
                if (queue.push(Event{producer, sequence})) {
                    accepted.fetch_add(1, std::memory_order_relaxed);
                } else {
                    rejected.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }
    begin.count_down();
    for (auto& producer : writers) {
        producer.join();
    }
    queue.close();
    for (auto& consumer : readers) {
        consumer.join();
    }

    require(accepted.load() == total, "all 100,000 events must be accepted");
    require(rejected.load() == 0, "no event rejected before close");
    require(received.load() == total, "every accepted event must be received");
    require(malformed.load() == 0, "event producer/sequence must be valid");
    for (const auto& count : seen) {
        require(count.load(std::memory_order_relaxed) == 1,
                "every event ID must be delivered exactly once");
    }
    require(queue.is_closed() && queue.size() == 0,
            "queue must be closed and drained");
    require(!queue.try_pop(), "try_pop after drain must return empty");
    require(!queue.push(Event{0, 0}), "push after close must be rejected");

    std::cout << "PASS: 100000 events; producers=4 consumers=4; "
                 "missing=0 duplicates=0 malformed=0 rejected=0\n";
}

// MPMC cannot globally assert per-producer delivery order at independent
// consumer callbacks: one consumer may be descheduled after dequeue. A
// single consumer gives a reliable observation order for this FIFO check.
void test_per_producer_fifo_with_concurrent_producers() {
    constexpr std::size_t producers = 4;
    constexpr std::size_t per_producer = 5'000;
    ThreadSafeQueue<Event> queue;
    std::vector<std::size_t> next_expected(producers, 0);
    bool ordered = true;
    std::size_t received = 0;
    std::jthread consumer([&] {
        while (auto event = queue.wait_pop()) {
            if (event->producer >= producers ||
                event->sequence != next_expected[event->producer]) {
                ordered = false;
            } else {
                ++next_expected[event->producer];
            }
            ++received;
        }
    });
    std::vector<std::jthread> writers;
    writers.reserve(producers);
    std::atomic<std::size_t> rejected{0};
    for (std::size_t producer = 0; producer < producers; ++producer) {
        writers.emplace_back([&, producer] {
            for (std::size_t seq = 0; seq < per_producer; ++seq) {
                if (!queue.push(Event{producer, seq})) {
                    rejected.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }
    for (auto& writer : writers) {
        writer.join();
    }
    queue.close();
    consumer.join();
    require(ordered, "per-producer FIFO must be preserved");
    require(received == producers * per_producer, "all FIFO events drained");
    require(rejected.load() == 0, "FIFO writes accepted before close");
    for (auto index : next_expected) {
        require(index == per_producer, "each producer fully ordered");
    }
    std::cout << "PASS: 20000 events; concurrent producers; per-producer FIFO\n";
}

// Close is raced against the second wave of producer pushes. Either result
// for individual concurrent pushes is allowed; every successful push must
// still be consumed once. Consumers only terminate after a complete drain.
void test_close_under_producer_contention_drains_accepted_events() {
    constexpr std::size_t producers = 4;
    constexpr std::size_t consumers = 4;
    constexpr std::size_t per_producer = 20'000;
    constexpr std::size_t prefix = 1'000;
    constexpr std::size_t capacity = producers * per_producer;
    ThreadSafeQueue<Event> queue;
    std::vector<std::atomic<unsigned>> seen(capacity);
    for (auto& counter : seen) {
        counter.store(0, std::memory_order_relaxed);
    }
    std::atomic<std::size_t> accepted{0};
    std::atomic<std::size_t> received{0};
    std::atomic<std::size_t> rejected{0};
    std::atomic<std::size_t> malformed{0};
    std::latch prefix_done(producers);
    std::latch second_wave(1);

    std::vector<std::jthread> readers;
    readers.reserve(consumers);
    for (std::size_t i = 0; i < consumers; ++i) {
        readers.emplace_back([&] {
            while (auto event = queue.wait_pop()) {
                if (event->producer >= producers ||
                    event->sequence >= per_producer) {
                    malformed.fetch_add(1, std::memory_order_relaxed);
                    continue;
                }
                const std::size_t id =
                    event->producer * per_producer + event->sequence;
                seen[id].fetch_add(1, std::memory_order_relaxed);
                received.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    std::vector<std::jthread> writers;
    writers.reserve(producers);
    for (std::size_t producer = 0; producer < producers; ++producer) {
        writers.emplace_back([&, producer] {
            for (std::size_t seq = 0; seq < prefix; ++seq) {
                if (queue.push(Event{producer, seq})) {
                    accepted.fetch_add(1, std::memory_order_relaxed);
                } else {
                    rejected.fetch_add(1, std::memory_order_relaxed);
                }
            }
            prefix_done.count_down();
            second_wave.wait();
            for (std::size_t seq = prefix; seq < per_producer; ++seq) {
                if (queue.push(Event{producer, seq})) {
                    accepted.fetch_add(1, std::memory_order_relaxed);
                } else {
                    rejected.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }
    prefix_done.wait();
    // Producers remain alive, poised for more pushes, at this boundary.
    second_wave.count_down();
    queue.close();
    queue.close(); // Repeated concurrent lifecycle calls must be safe.
    for (auto& writer : writers) {
        writer.join();
    }
    for (auto& reader : readers) {
        reader.join();
    }

    const auto accepted_total = accepted.load();
    const auto rejected_total = rejected.load();
    require(accepted_total >= producers * prefix,
            "prefix accepted before close");
    require(accepted_total + rejected_total == capacity,
            "every attempted push has one outcome");
    require(received.load() == accepted_total,
            "close must drain every successful push");
    require(malformed.load() == 0, "all consumed events valid");
    for (const auto& value : seen) {
        require(value.load(std::memory_order_relaxed) <= 1,
                "no accepted ID delivered twice");
    }
    require(!queue.push(Event{0, 0}), "post-close push must fail");
    require(queue.is_closed() && queue.size() == 0,
            "close must leave a drained queue");
    std::cout << "PASS: contended close; accepted=" << accepted_total
              << " drained=" << received.load() << " rejected="
              << rejected_total << " duplicates=0\n";
}

// Deliberate task exceptions must not terminate the worker or lose accepted
// tasks. This is not an application-layer GUI/PJSIP error propagation test.
void test_worker_exception_injection_under_many_posts() {
    constexpr std::size_t producers = 4;
    constexpr std::size_t per_producer = 2'500;
    constexpr std::size_t total = producers * per_producer;
    constexpr std::size_t throw_every = 97;
    SerialTaskWorker worker;
    std::atomic<std::size_t> executed{0};
    std::atomic<std::size_t> rejected{0};
    std::size_t expected_failures = 0;
    for (std::size_t id = 0; id < total; ++id) {
        if (id % throw_every == 0) {
            ++expected_failures;
        }
    }
    std::vector<std::jthread> writers;
    writers.reserve(producers);
    for (std::size_t producer = 0; producer < producers; ++producer) {
        writers.emplace_back([&, producer] {
            for (std::size_t seq = 0; seq < per_producer; ++seq) {
                const auto id = producer * per_producer + seq;
                if (!worker.post([&, id] {
                        executed.fetch_add(1, std::memory_order_relaxed);
                        if (id % throw_every == 0) {
                            throw std::runtime_error("intentional stress injection");
                        }
                    })) {
                    rejected.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }
    for (auto& writer : writers) {
        writer.join();
    }
    worker.shutdown();
    require(rejected.load() == 0, "all worker tasks accepted before shutdown");
    require(executed.load() == total, "every worker task executed once");
    require(worker.completed() == total, "worker completion accounting");
    require(worker.failures() == expected_failures,
            "all injected exceptions contained and counted");
    require(!worker.post([] {}), "worker rejects tasks after shutdown");
    std::cout << "PASS: 10000 worker tasks; injected failures="
              << expected_failures << "; completed=10000\n";
}

} // namespace

int main() {
    try {
        test_100000_event_mpmc_integrity();
        test_per_producer_fifo_with_concurrent_producers();
        test_close_under_producer_contention_drains_accepted_events();
        test_worker_exception_injection_under_many_posts();
        std::cout << "PASS: 4 Phase 8 MPMC stress scenarios\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
