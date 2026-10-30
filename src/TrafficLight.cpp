#include "TrafficLight.h"

#include <chrono>
#include <random>
#include <thread>

using namespace std::chrono_literals;

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
    threads_.emplace_back(
        [this](std::stop_token st) { cycleThroughPhases(st); });
}

void TrafficLight::cycleThroughPhases(std::stop_token stop_token) {
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> cycle_ms(4000, 6000);

    auto last_switch = std::chrono::steady_clock::now();
    std::chrono::milliseconds cycle_duration(cycle_ms(rng));

    while (!stop_token.stop_requested()) {
        std::this_thread::sleep_for(1ms);

        if (std::chrono::steady_clock::now() - last_switch < cycle_duration) {
            continue;
        }

        const TrafficLightPhase new_phase =
            current_phase_.load() == TrafficLightPhase::kRed
                ? TrafficLightPhase::kGreen
                : TrafficLightPhase::kRed;
        current_phase_.store(new_phase);
        phase_queue_.send(new_phase);

        log("TrafficLight #", id_, " switched to ",
            new_phase == TrafficLightPhase::kGreen ? "green" : "red");

        cycle_duration = std::chrono::milliseconds(cycle_ms(rng));
        last_switch = std::chrono::steady_clock::now();
    }
}
