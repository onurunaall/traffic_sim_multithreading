#include "Intersection.h"

#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <thread>
#include <vector>

TEST(WaitingVehiclesTest, InitialSizeIsZero) {
    WaitingVehicles wv;
    EXPECT_EQ(wv.getSize(), 0);
}

TEST(WaitingVehiclesTest, PushBackIncreasesSize) {
    WaitingVehicles wv;
    std::promise<void> p;
    wv.pushBack(1, std::move(p));
    EXPECT_EQ(wv.getSize(), 1);
}

TEST(WaitingVehiclesTest, PermitEntryFulfillsPromise) {
    WaitingVehicles wv;

    std::promise<void> p;
    std::future<void> f = p.get_future();
    wv.pushBack(1, std::move(p));

    EXPECT_EQ(wv.getSize(), 1);

    EXPECT_TRUE(wv.permitEntryToFirstInQueue());

    // The future should now be ready (promise fulfilled)
    auto status = f.wait_for(std::chrono::milliseconds(100));
    EXPECT_EQ(status, std::future_status::ready);
    EXPECT_EQ(wv.getSize(), 0);
}

TEST(WaitingVehiclesTest, FIFO_Order) {
    WaitingVehicles wv;

    std::promise<void> p1, p2;
    std::future<void> f1 = p1.get_future();
    std::future<void> f2 = p2.get_future();

    wv.pushBack(1, std::move(p1));
    wv.pushBack(2, std::move(p2));
    EXPECT_EQ(wv.getSize(), 2);

    // First permit should fulfill p1 (first in)
    wv.permitEntryToFirstInQueue();
    EXPECT_EQ(f1.wait_for(std::chrono::milliseconds(100)),
              std::future_status::ready);
    // f2 should NOT be ready yet
    EXPECT_EQ(f2.wait_for(std::chrono::milliseconds(10)),
              std::future_status::timeout);

    // Second permit fulfills p2
    wv.permitEntryToFirstInQueue();
    EXPECT_EQ(f2.wait_for(std::chrono::milliseconds(100)),
              std::future_status::ready);
    EXPECT_EQ(wv.getSize(), 0);
}

TEST(WaitingVehiclesTest, PermitOnEmptyQueueIsNoOp) {
    WaitingVehicles wv;
    // Should not crash or throw, and reports that nobody was admitted
    EXPECT_FALSE(wv.permitEntryToFirstInQueue());
    EXPECT_EQ(wv.getSize(), 0);
}

TEST(WaitingVehiclesTest, RemoveUnblocksWithoutGrantingEntry) {
    WaitingVehicles wv;

    std::promise<void> p1, p2;
    std::future<void> f1 = p1.get_future();
    std::future<void> f2 = p2.get_future();
    wv.pushBack(1, std::move(p1));
    wv.pushBack(2, std::move(p2));

    wv.remove(1);
    EXPECT_EQ(wv.getSize(), 1);

    // The removed vehicle's future is ready with broken_promise
    ASSERT_EQ(f1.wait_for(std::chrono::milliseconds(0)),
              std::future_status::ready);
    EXPECT_THROW(f1.get(), std::future_error);

    // The other vehicle is still queued and is next in line
    EXPECT_EQ(f2.wait_for(std::chrono::milliseconds(0)),
              std::future_status::timeout);
    wv.permitEntryToFirstInQueue();
    EXPECT_NO_THROW(f2.get());
}

TEST(WaitingVehiclesTest, ConcurrentPushBack) {
    WaitingVehicles wv;
    constexpr int kNumThreads = 16;

    std::vector<std::jthread> threads;
    for (int i = 0; i < kNumThreads; ++i) {
        threads.emplace_back([&wv, i] {
            std::promise<void> p;
            wv.pushBack(i, std::move(p));
        });
    }
    threads.clear();  // join all

    EXPECT_EQ(wv.getSize(), kNumThreads);
}
