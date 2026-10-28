#include "Intersection.h"
#include "Street.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

/* ── WaitingVehicles ──────────────────────────────────────────────── */

int WaitingVehicles::getSize() {
    std::lock_guard<std::mutex> lock(mtx_);
    return static_cast<int>(entries_.size());
}

void WaitingVehicles::pushBack(int vehicle_id, std::promise<void> &&promise) {
    std::lock_guard<std::mutex> lock(mtx_);
    entries_.push_back({vehicle_id, std::move(promise)});
}

bool WaitingVehicles::permitEntryToFirstInQueue() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (entries_.empty()) return false;

    // Fulfill promise → unblocks the waiting vehicle thread
    entries_.front().promise.set_value();
    entries_.pop_front();
    return true;
}

void WaitingVehicles::remove(int vehicle_id) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = std::find_if(entries_.begin(), entries_.end(),
                           [vehicle_id](const Entry &e) {
                               return e.vehicle_id == vehicle_id;
                           });
    if (it != entries_.end()) {
        entries_.erase(it);
    }
}

/* ── Intersection ─────────────────────────────────────────────────── */

Intersection::Intersection() {
    type_ = ObjectType::kIntersection;
}

Intersection::~Intersection() {
    // Join the queue processor before waiting_vehicles_ is destroyed.
    stopThreads();
}

void Intersection::addStreet(const std::shared_ptr<Street> &street) {
    streets_.push_back(street);
}

std::vector<std::shared_ptr<Street>>
Intersection::queryStreets(const std::shared_ptr<Street> &incoming) const {
    std::vector<std::shared_ptr<Street>> outgoing;
    for (const auto &weak_street : streets_) {
        auto s = weak_street.lock();
        if (s && s->getID() != incoming->getID()) {
            outgoing.push_back(std::move(s));
        }
    }
    return outgoing;
}

bool Intersection::addVehicleToQueue(int vehicle_id, std::stop_token stop_token) {
    {
        std::lock_guard<std::mutex> lock(cout_mtx_);
        std::cout << "Intersection #" << id_
                  << "::addVehicleToQueue: thread id = "
                  << std::this_thread::get_id() << "\n";
    }

    // Enqueue vehicle with a promise/future pair for synchronization
    std::promise<void> prms;
    std::future<void> ftr = prms.get_future();
    waiting_vehicles_.pushBack(vehicle_id, std::move(prms));

    {
        // If stop is requested while queued, drop the entry: destroying its
        // promise makes ftr ready, so the wait below returns.
        std::stop_callback on_stop(stop_token, [this, vehicle_id] {
            waiting_vehicles_.remove(vehicle_id);
        });

        // Block until the intersection grants entry
        ftr.wait();
    }

    // Stop only happens at shutdown, so a vehicle stopped after being granted
    // entry may leave the intersection blocked.
    if (stop_token.stop_requested()) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(cout_mtx_);
        std::cout << "Intersection #" << id_ << ": Vehicle #"
                  << vehicle_id << " is granted entry.\n";
    }

    // Block until the traffic light is green
    return traffic_light_.waitForGreen(stop_token);
}

void Intersection::vehicleHasLeft() {
    num_crossings_.fetch_add(1);
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
            // Block before granting so the vehicle's vehicleHasLeft() can't
            // run before this store. If a stopping vehicle emptied the queue
            // in the meantime, undo the block.
            is_blocked_.store(true);
            if (!waiting_vehicles_.permitEntryToFirstInQueue()) {
                is_blocked_.store(false);
            }
        }
    }
}
