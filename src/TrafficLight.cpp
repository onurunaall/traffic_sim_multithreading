#include "TrafficLight.h"

#include <chrono>
#include <iostream>
#include <random>
#include <thread>

TrafficLight::TrafficLight() : current_phase_(TrafficLightPhase::kRed) {}

TrafficLight::~TrafficLight() {
    // Join the cycle thread before phase_queue_ is destroyed.
    stopThreads();
}

TrafficLightPhase TrafficLight::getCurrentPhase() const {
    return current_phase_.load();
}

bool TrafficLight::waitForGreen(std::stop_token stop_token) {
    // current_phase_ is the source of truth; queue messages only wake us up.
    // Re-checking the phase after every message means a stale green left in
    // the queue cannot let a vehicle through on red. The phase is stored
    // before the message is sent, so no wake-up is lost.
    while (current_phase_.load() != TrafficLightPhase::kGreen) {
        if (!phase_queue_.receive(stop_token)) {
            return false;
        }
    }
    return true;
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

            // Send update to message queue
            phase_queue_.send(new_phase);

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
