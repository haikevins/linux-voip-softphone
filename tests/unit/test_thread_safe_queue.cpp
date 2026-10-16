#include "threading/ThreadSafeQueue.hpp"

#include <gtest/gtest.h>

#include <future>
#include <memory>
#include <optional>

namespace {

using voip::threading::ThreadSafeQueue;

} // namespace

TEST(ThreadSafeQueueTest, PreservesFifoOrderAndTracksSize)
{
    ThreadSafeQueue<int> queue;

    EXPECT_TRUE(queue.push(10));
    EXPECT_TRUE(queue.push(20));
    EXPECT_EQ(queue.size(), 2U);

    EXPECT_EQ(queue.wait_and_pop(), std::optional<int>{10});
    EXPECT_EQ(queue.wait_and_pop(), std::optional<int>{20});
    EXPECT_EQ(queue.size(), 0U);
}

TEST(ThreadSafeQueueTest, WakesWaitingConsumerWhenValueArrives)
{
    ThreadSafeQueue<int> queue;
    auto consumer = std::async(std::launch::async, [&queue]() { return queue.wait_and_pop(); });

    EXPECT_TRUE(queue.push(42));
    EXPECT_EQ(consumer.get(), std::optional<int>{42});
}

TEST(ThreadSafeQueueTest, CloseWakesWaitingConsumerWithoutValue)
{
    ThreadSafeQueue<int> queue;
    auto consumer = std::async(std::launch::async, [&queue]() { return queue.wait_and_pop(); });

    queue.close();

    EXPECT_FALSE(consumer.get().has_value());
    EXPECT_TRUE(queue.closed());
}

TEST(ThreadSafeQueueTest, RejectsPushAfterClose)
{
    ThreadSafeQueue<int> queue;
    queue.close();

    EXPECT_FALSE(queue.push(1));
    EXPECT_EQ(queue.size(), 0U);
}

TEST(ThreadSafeQueueTest, SupportsMoveOnlyValues)
{
    ThreadSafeQueue<std::unique_ptr<int>> queue;

    EXPECT_TRUE(queue.push(std::make_unique<int>(7)));
    auto value = queue.wait_and_pop();

    EXPECT_TRUE(value.has_value());
    auto pointer = std::move(value).value_or(nullptr);

    ASSERT_NE(pointer, nullptr);
    EXPECT_EQ(*pointer, 7);
}
