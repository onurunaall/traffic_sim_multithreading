#include "TrafficLight.h"

#include <chrono>
#include <iostream>
#include <random>
#include <thread>

TrafficLight::TrafficLight() : current_phase_(TrafficLightPhase::kRed) {
    type_ = ObjectType::kIntersection; // rendered at intersection position
}

TrafficLightPhase TrafficLight::getCurrentPhase() const {
    return current_phase_.load();
}

void TrafficLight::waitForGreen() {
    // Block until the message queue delivers a green phase.
    while (true) {
        TrafficLightPhase phase = phase_queue_.receive();
        if (phase == TrafficLightPhase::kGreen) {
            return;
        }
    }
}

void TrafficLight::simulate() {
    // jthread automatically provides a stop_token to callables whose first
    // parameter is std::stop_token — enabling cooperative cancellation.
    threads_.emplace_back(std::jthread([this](std::stop_token st) {
        cycleThroughPhases(st);
    }));
}

void TrafficLight::cycleThroughPhases(std::stop_token stop_token) {
    // Random cycle duration between 4000 and 6000 ms
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<int> dist(4000, 6000);

    auto last_switch = std::chrono::steady_clock::now();
    int cycle_duration_ms = dist(rng);

    while (!stop_token.stop_requested()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - last_switch)
                            .count();

        if (elapsed >= cycle_duration_ms) {
            // Toggle phase
            TrafficLightPhase new_phase =
                (current_phase_.load() == TrafficLightPhase::kRed)
                    ? TrafficLightPhase::kGreen
                    : TrafficLightPhase::kRed;

            current_phase_.store(new_phase);

            // Send update to message queue (move semantics)
            phase_queue_.send(std::move(new_phase));

            // Log the change
            {
                std::lock_guard<std::mutex> lock(cout_mtx_);
                std::cout << "TrafficLight #" << id_ << " switched to "
                          << (new_phase == TrafficLightPhase::kGreen ? "green"
                                                                     : "red")
                          << "\n";
            }

            // Pick new random duration for next cycle
            cycle_duration_ms = dist(rng);
            last_switch = std::chrono::steady_clock::now();
        }
    }
}
