#include "Intersection.h"
#include "Street.h"

#include <chrono>
#include <thread>

using namespace std::chrono_literals;

/* ── WaitingVehicles ──────────────────────────────────────────────── */

int WaitingVehicles::getSize() {
    std::lock_guard lock(mtx_);
    return static_cast<int>(entries_.size());
}

void WaitingVehicles::pushBack(int vehicle_id, std::promise<void> &&promise) {
    std::lock_guard lock(mtx_);
    entries_.push_back({vehicle_id, std::move(promise)});
}

bool WaitingVehicles::permitEntryToFirstInQueue() {
    std::lock_guard lock(mtx_);
    if (entries_.empty()) return false;

    // Fulfill promise → unblocks the waiting vehicle thread
    entries_.front().promise.set_value();
    entries_.pop_front();
    return true;
}

void WaitingVehicles::remove(int vehicle_id) {
    std::lock_guard lock(mtx_);
    std::erase_if(entries_, [vehicle_id](const Entry &e) {
        return e.vehicle_id == vehicle_id;
    });
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
        if (s && s != incoming) {
            outgoing.push_back(std::move(s));
        }
    }
    return outgoing;
}

bool Intersection::addVehicleToQueue(int vehicle_id, std::stop_token stop_token) {
    log("Intersection #", id_, "::addVehicleToQueue: thread id = ",
        std::this_thread::get_id());

    std::promise<void> entry_permit;
    std::future<void> entry_granted = entry_permit.get_future();
    waiting_vehicles_.pushBack(vehicle_id, std::move(entry_permit));

    {
        // If stop is requested while queued, drop the entry: destroying its
        // promise makes entry_granted ready, so the wait below returns.
        std::stop_callback on_stop(stop_token, [this, vehicle_id] {
            waiting_vehicles_.remove(vehicle_id);
        });
        entry_granted.wait();
    }

    // Stop only happens at shutdown, so a vehicle stopped after being granted
    // entry may leave the intersection blocked.
    if (stop_token.stop_requested()) {
        return false;
    }

    log("Intersection #", id_, ": Vehicle #", vehicle_id, " is granted entry.");

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
    traffic_light_.simulate();
    threads_.emplace_back(
        [this](std::stop_token st) { processVehicleQueue(st); });
}

void Intersection::processVehicleQueue(std::stop_token stop_token) {
    while (!stop_token.stop_requested()) {
        std::this_thread::sleep_for(1ms);

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
