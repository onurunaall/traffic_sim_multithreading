#include "TrafficLight.h"

#include <gtest/gtest.h>

#include <chrono>
#include <future>

TEST(TrafficLightTest, InitialPhaseIsRed) {
    TrafficLight tl;
    EXPECT_EQ(tl.getCurrentPhase(), TrafficLightPhase::kRed);
}

TEST(TrafficLightTest, PhaseTogglesAfterSimulate) {
    TrafficLight tl;
    EXPECT_EQ(tl.getCurrentPhase(), TrafficLightPhase::kRed);

    tl.simulate();

    // The light should toggle within 4–6 seconds. Wait up to 7s.
    auto start = std::chrono::steady_clock::now();
    while (tl.getCurrentPhase() == TrafficLightPhase::kRed) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed > std::chrono::seconds(7)) {
            FAIL() << "TrafficLight did not switch from red within 7 seconds";
        }
    }
    EXPECT_EQ(tl.getCurrentPhase(), TrafficLightPhase::kGreen);

    // Verify it eventually returns to red (another 4–6s cycle)
    start = std::chrono::steady_clock::now();
    while (tl.getCurrentPhase() == TrafficLightPhase::kGreen) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed > std::chrono::seconds(7)) {
            FAIL() << "TrafficLight did not switch from green within 7 seconds";
        }
    }
    EXPECT_EQ(tl.getCurrentPhase(), TrafficLightPhase::kRed);
}

TEST(TrafficLightTest, WaitForGreenReturnsOnGreen) {
    TrafficLight tl;
    tl.simulate();

    // waitForGreen should return within ~7 seconds (at most one full red cycle)
    auto future = std::async(std::launch::async, [&tl] {
        tl.waitForGreen();
    });

    auto status = future.wait_for(std::chrono::seconds(8));
    ASSERT_EQ(status, std::future_status::ready)
        << "waitForGreen() did not return within 8 seconds — possible deadlock";
}

TEST(TrafficLightTest, CycleDurationWithinExpectedRange) {
    TrafficLight tl;
    tl.simulate();

    // Wait for the first toggle (red → green)
    auto start = std::chrono::steady_clock::now();
    while (tl.getCurrentPhase() == TrafficLightPhase::kRed) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (std::chrono::steady_clock::now() - start > std::chrono::seconds(8)) {
            FAIL() << "Timeout waiting for first toggle";
        }
    }
    auto first_toggle = std::chrono::steady_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            first_toggle - start)
                            .count();

    // Cycle should be between 4000 and 6000 ms, with some tolerance for
    // scheduling jitter
    EXPECT_GE(duration_ms, 3900) << "Cycle too short";
    EXPECT_LE(duration_ms, 6200) << "Cycle too long";
}

TEST(TrafficLightTest, DestructionDuringSimulationDoesNotHang) {
    // Verify graceful shutdown: destructor should not deadlock
    auto start = std::chrono::steady_clock::now();
    {
        TrafficLight tl;
        tl.simulate();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        // tl goes out of scope here — destructor requests stop and joins
    }
    auto elapsed = std::chrono::steady_clock::now() - start;
    // Should complete within 1 second (not hang forever)
    EXPECT_LT(elapsed, std::chrono::seconds(2))
        << "TrafficLight destructor took too long — possible deadlock";
}
