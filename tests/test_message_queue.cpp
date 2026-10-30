#include "MessageQueue.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <thread>
#include <vector>

// ─── Basic functionality ─────────────────────────────────────────────

TEST(MessageQueueTest, SendAndReceiveSingleMessage) {
    MessageQueue<int> q;
    q.send(42);
    EXPECT_EQ(q.receive(), 42);
}

TEST(MessageQueueTest, FIFO_Ordering) {
    MessageQueue<int> q(10);  // capacity 10 so nothing gets dropped
    q.send(1);
    q.send(2);
    q.send(3);
    EXPECT_EQ(q.receive(), 1);
    EXPECT_EQ(q.receive(), 2);
    EXPECT_EQ(q.receive(), 3);
}

TEST(MessageQueueTest, BoundedCapacityDropsOldest) {
    MessageQueue<int> q(2);  // keep at most 2
    q.send(1);
    q.send(2);
    q.send(3);  // should drop 1
    EXPECT_EQ(q.receive(), 2);
    EXPECT_EQ(q.receive(), 3);
}

TEST(MessageQueueTest, DefaultCapacityIsOne) {
    MessageQueue<int> q;  // default max_size = 1
    q.send(1);
    q.send(2);  // should drop 1
    EXPECT_EQ(q.receive(), 2);
}

TEST(MessageQueueTest, MoveSemantics) {
    MessageQueue<std::unique_ptr<int>> q;
    q.send(std::make_unique<int>(99));
    auto ptr = q.receive();
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(*ptr, 99);
}

// ─── Blocking behavior ──────────────────────────────────────────────

TEST(MessageQueueTest, ReceiveBlocksUntilSend) {
    MessageQueue<int> q;
    std::atomic<bool> received{false};

    auto future = std::async(std::launch::async, [&] {
        int val = q.receive();
        received.store(true);
        return val;
    });

    // Give the receiver time to block
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_FALSE(received.load());

    q.send(7);
    EXPECT_EQ(future.get(), 7);
    EXPECT_TRUE(received.load());
}

TEST(MessageQueueTest, ReceiveWithStopTokenReturnsMessage) {
    MessageQueue<int> q;
    std::stop_source stop;
    q.send(5);
    auto val = q.receive(stop.get_token());
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, 5);
}

TEST(MessageQueueTest, StopUnblocksReceive) {
    MessageQueue<int> q;
    std::stop_source stop;

    std::promise<bool> got_message;
    auto got_message_ftr = got_message.get_future();
    std::jthread receiver([&] {
        got_message.set_value(q.receive(stop.get_token()).has_value());
    });

    EXPECT_EQ(got_message_ftr.wait_for(std::chrono::milliseconds(50)),
              std::future_status::timeout);

    stop.request_stop();
    ASSERT_EQ(got_message_ftr.wait_for(std::chrono::seconds(1)),
              std::future_status::ready)
        << "Stop request did not unblock receive()";
    EXPECT_FALSE(got_message_ftr.get());
}

// ─── Thread safety ──────────────────────────────────────────────────

TEST(MessageQueueTest, ConcurrentProducersSingleConsumer) {
    constexpr int kNumProducers = 8;
    constexpr int kMessagesPerProducer = 1000;
    MessageQueue<int> q(kNumProducers * kMessagesPerProducer);

    // Launch producers
    std::vector<std::jthread> producers;
    for (int p = 0; p < kNumProducers; ++p) {
        producers.emplace_back([&q, p] {
            for (int i = 0; i < kMessagesPerProducer; ++i) {
                int val = p * kMessagesPerProducer + i;
                q.send(std::move(val));
            }
        });
    }

    // Wait for all producers to finish
    producers.clear();

    // Consume all messages
    std::vector<int> received;
    received.reserve(kNumProducers * kMessagesPerProducer);
    for (int i = 0; i < kNumProducers * kMessagesPerProducer; ++i) {
        received.push_back(q.receive());
    }

    EXPECT_EQ(static_cast<int>(received.size()),
              kNumProducers * kMessagesPerProducer);

    // Every message should appear exactly once
    std::sort(received.begin(), received.end());
    for (int i = 0; i < kNumProducers * kMessagesPerProducer; ++i) {
        EXPECT_EQ(received[static_cast<std::size_t>(i)], i);
    }
}

TEST(MessageQueueTest, ConcurrentProducerConsumerPingPong) {
    // One producer, one consumer, interleaved — tests for deadlock/livelock.
    // With bounded queue, some messages may be dropped, so the consumer
    // only checks that it receives values in strictly increasing order
    // (monotonicity) rather than expecting all 5000.
    constexpr int kNumMessages = 5000;
    constexpr std::size_t kCapacity = 100;
    MessageQueue<int> q(kCapacity);

    // send() never blocks, so the producer cannot hang.
    auto producer = std::async(std::launch::async, [&] {
        for (int i = 0; i < kNumMessages; ++i) {
            q.send(i);
        }
        // Send sentinel to unblock consumer
        q.send(kNumMessages);
    });

    // The consumer runs on a jthread with a stoppable receive so that, if an
    // assertion below fails, the jthread's destructor unblocks it.
    std::promise<int> consumer_count;
    auto consumer_ftr = consumer_count.get_future();
    std::jthread consumer([&](std::stop_token st) {
        int last = -1;
        int count = 0;
        while (auto val = q.receive(st)) {
            if (*val == kNumMessages) break;  // sentinel
            EXPECT_GT(*val, last) << "Non-monotonic at count " << count;
            last = *val;
            ++count;
        }
        consumer_count.set_value(count);
    });

    ASSERT_EQ(producer.wait_for(std::chrono::seconds(5)),
              std::future_status::ready);
    ASSERT_EQ(consumer_ftr.wait_for(std::chrono::seconds(5)),
              std::future_status::ready);

    int received = consumer_ftr.get();
    // We should receive at least some messages (not zero, not all if drops occur)
    EXPECT_GT(received, 0);
}
