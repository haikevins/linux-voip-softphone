#include "threading/ThreadSafeQueue.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <cstddef>
#include <latch>
#include <thread>
#include <vector>

namespace {

using voip::threading::ThreadSafeQueue;

constexpr std::size_t kEventCount = 100000;
constexpr std::size_t kProducerCount = 4;
constexpr std::size_t kConsumerCount = 4;
static_assert(kEventCount % kProducerCount == 0);

} // namespace

TEST(ThreadSafeQueueStressTest, TransfersOneHundredThousandEventsExactlyOnce)
{
    ThreadSafeQueue<std::size_t> queue;
    std::vector<std::atomic<unsigned int>> seen(kEventCount);
    for (auto &counter : seen) {
        counter.store(0U, std::memory_order_relaxed);
    }

    std::atomic<std::size_t> consumed_events{0};
    std::atomic<std::size_t> failed_pushes{0};
    std::atomic<std::size_t> invalid_events{0};
    std::latch start_gate{kProducerCount + kConsumerCount};

    std::vector<std::thread> consumers;
    consumers.reserve(kConsumerCount);
    for (std::size_t index = 0; index < kConsumerCount; ++index) {
        consumers.emplace_back([&]() {
            start_gate.arrive_and_wait();
            while (auto event = queue.wait_and_pop()) {
                if (*event >= kEventCount) {
                    invalid_events.fetch_add(1, std::memory_order_relaxed);
                    continue;
                }
                seen[*event].fetch_add(1U, std::memory_order_relaxed);
                consumed_events.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    constexpr std::size_t events_per_producer = kEventCount / kProducerCount;
    std::vector<std::thread> producers;
    producers.reserve(kProducerCount);
    for (std::size_t index = 0; index < kProducerCount; ++index) {
        const auto begin = index * events_per_producer;
        const auto end = begin + events_per_producer;
        producers.emplace_back([&, begin, end]() {
            start_gate.arrive_and_wait();
            for (auto event = begin; event < end; ++event) {
                if (!queue.push(event)) {
                    failed_pushes.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    for (auto &producer : producers) {
        producer.join();
    }
    queue.close();
    for (auto &consumer : consumers) {
        consumer.join();
    }

    std::size_t missing_events = 0;
    std::size_t duplicate_events = 0;
    for (const auto &counter : seen) {
        const auto count = counter.load(std::memory_order_relaxed);
        if (count == 0U) {
            ++missing_events;
        } else if (count > 1U) {
            duplicate_events += count - 1U;
        }
    }

    EXPECT_EQ(failed_pushes.load(std::memory_order_relaxed), 0U);
    EXPECT_EQ(invalid_events.load(std::memory_order_relaxed), 0U);
    EXPECT_EQ(consumed_events.load(std::memory_order_relaxed), kEventCount);
    EXPECT_EQ(missing_events, 0U);
    EXPECT_EQ(duplicate_events, 0U);
    EXPECT_EQ(queue.size(), 0U);
    EXPECT_TRUE(queue.closed());
}
