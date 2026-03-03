#include "Intersection.h"
#include "Street.h"
#include "Vehicle.h"

#include <chrono>
#include <iostream>
#include <thread>

/* ── WaitingVehicles ──────────────────────────────────────────────── */

int WaitingVehicles::getSize() {
    std::lock_guard<std::mutex> lock(mtx_);
    return static_cast<int>(vehicles_.size());
}

void WaitingVehicles::pushBack(std::shared_ptr<Vehicle> vehicle,
                                std::promise<void> &&promise) {
    std::lock_guard<std::mutex> lock(mtx_);
    vehicles_.push_back(std::move(vehicle));
    promises_.push_back(std::move(promise));
}

void WaitingVehicles::permitEntryToFirstInQueue() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (vehicles_.empty()) return;

    // Fulfill promise → unblocks the waiting vehicle thread
    promises_.front().set_value();

    vehicles_.pop_front();
    promises_.pop_front();
}

/* ── Intersection ─────────────────────────────────────────────────── */

Intersection::Intersection() {
    type_ = ObjectType::kIntersection;
}

void Intersection::addStreet(std::shared_ptr<Street> street) {
    streets_.push_back(std::move(street));
}

std::vector<std::shared_ptr<Street>>
Intersection::queryStreets(const std::shared_ptr<Street> &incoming) const {
    std::vector<std::shared_ptr<Street>> outgoing;
    for (const auto &s : streets_) {
        if (s->getID() != incoming->getID()) {
            outgoing.push_back(s);
        }
    }
    return outgoing;
}

void Intersection::addVehicleToQueue(std::shared_ptr<Vehicle> vehicle) {
    const int vehicle_id = vehicle->getID();

    {
        std::lock_guard<std::mutex> lock(cout_mtx_);
        std::cout << "Intersection #" << id_
                  << "::addVehicleToQueue: thread id = "
                  << std::this_thread::get_id() << "\n";
    }

    // Enqueue vehicle with a promise/future pair for synchronization
    std::promise<void> prms;
    std::future<void> ftr = prms.get_future();
    waiting_vehicles_.pushBack(std::move(vehicle), std::move(prms));

    // Block until the intersection grants entry
    ftr.wait();

    {
        std::lock_guard<std::mutex> lock(cout_mtx_);
        std::cout << "Intersection #" << id_ << ": Vehicle #"
                  << vehicle_id << " is granted entry.\n";
    }

    // Block until the traffic light is green
    if (traffic_light_.getCurrentPhase() == TrafficLightPhase::kRed) {
        traffic_light_.waitForGreen();
    }
}

void Intersection::vehicleHasLeft(std::shared_ptr<Vehicle> /*vehicle*/) {
    setIsBlocked(false);
}

void Intersection::setIsBlocked(bool blocked) {
    is_blocked_.store(blocked);
}

bool Intersection::trafficLightIsGreen() const {
    return traffic_light_.getCurrentPhase() == TrafficLightPhase::kGreen;
}

void Intersection::simulate() {
    // Start the traffic light cycle
    traffic_light_.simulate();

    // Launch the vehicle queue processor
    threads_.emplace_back(std::jthread([this](std::stop_token st) {
        processVehicleQueue(st);
    }));
}

void Intersection::processVehicleQueue(std::stop_token stop_token) {
    while (!stop_token.stop_requested()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        if (waiting_vehicles_.getSize() > 0 && !is_blocked_.load()) {
            is_blocked_.store(true);
            waiting_vehicles_.permitEntryToFirstInQueue();
        }
    }
}
