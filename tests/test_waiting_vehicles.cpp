#include "Intersection.h"
#include "Vehicle.h"

#include <gtest/gtest.h>

#include <future>
#include <memory>

TEST(WaitingVehiclesTest, InitialSizeIsZero) {
    WaitingVehicles wv;
    EXPECT_EQ(wv.getSize(), 0);
}

TEST(WaitingVehiclesTest, PushBackIncreasesSize) {
    WaitingVehicles wv;
    auto v = std::make_shared<Vehicle>();
    std::promise<void> p;
    wv.pushBack(v, std::move(p));
    EXPECT_EQ(wv.getSize(), 1);
}

TEST(WaitingVehiclesTest, PermitEntryFulfillsPromise) {
    WaitingVehicles wv;

    auto v = std::make_shared<Vehicle>();
    std::promise<void> p;
    std::future<void> f = p.get_future();
    wv.pushBack(v, std::move(p));

    EXPECT_EQ(wv.getSize(), 1);

    wv.permitEntryToFirstInQueue();

    // The future should now be ready (promise fulfilled)
    auto status = f.wait_for(std::chrono::milliseconds(100));
    EXPECT_EQ(status, std::future_status::ready);
    EXPECT_EQ(wv.getSize(), 0);
}

TEST(WaitingVehiclesTest, FIFO_Order) {
    WaitingVehicles wv;

    auto v1 = std::make_shared<Vehicle>();
    auto v2 = std::make_shared<Vehicle>();

    std::promise<void> p1, p2;
    std::future<void> f1 = p1.get_future();
    std::future<void> f2 = p2.get_future();

    wv.pushBack(v1, std::move(p1));
    wv.pushBack(v2, std::move(p2));
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
    // Should not crash or throw
    EXPECT_NO_THROW(wv.permitEntryToFirstInQueue());
    EXPECT_EQ(wv.getSize(), 0);
}

TEST(WaitingVehiclesTest, ConcurrentPushBack) {
    WaitingVehicles wv;
    constexpr int kNumThreads = 16;

    std::vector<std::jthread> threads;
    for (int i = 0; i < kNumThreads; ++i) {
        threads.emplace_back([&wv] {
            auto v = std::make_shared<Vehicle>();
            std::promise<void> p;
            wv.pushBack(v, std::move(p));
        });
    }
    threads.clear();  // join all

    EXPECT_EQ(wv.getSize(), kNumThreads);
}
