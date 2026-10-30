#include "Vehicle.h"
#include "Intersection.h"
#include "Street.h"

#include <chrono>
#include <thread>

using namespace std::chrono_literals;

Vehicle::Vehicle() : rng_(std::random_device{}()) {
    type_ = ObjectType::kVehicle;
}

Vehicle::~Vehicle() {
    // Join the drive thread before curr_street_, curr_destination_ and rng_
    // are destroyed. Stopping also wakes the thread if it is blocked in
    // Intersection::addVehicleToQueue.
    stopThreads();
}

void Vehicle::setCurrentDestination(std::shared_ptr<Intersection> destination) {
    curr_destination_ = std::move(destination);
    pos_street_ = 0.0;
}

void Vehicle::simulate() {
    threads_.emplace_back([this](std::stop_token st) { drive(st); });
}

void Vehicle::drive(std::stop_token stop_token) {
    log("Vehicle #", id_, "::drive: thread id = ", std::this_thread::get_id());

    auto last_update = std::chrono::steady_clock::now();

    while (!stop_token.stop_requested()) {
        std::this_thread::sleep_for(1ms);

        const auto now = std::chrono::steady_clock::now();
        const double dt_seconds =
            std::chrono::duration<double>(now - last_update).count();
        const double speed =
            is_in_intersection_ ? base_speed_ / 10.0 : base_speed_;
        pos_street_ += speed * dt_seconds;

        const double completion = pos_street_ / curr_street_->getLength();

        double x1, y1, x2, y2;
        curr_street_->getOtherEnd(curr_destination_)->getPosition(x1, y1);
        curr_destination_->getPosition(x2, y2);
        setPosition(x1 + completion * (x2 - x1), y1 + completion * (y2 - y1));

        // Approaching destination — request entry. Called directly on this
        // thread so it can be woken by our stop_token at shutdown.
        if (completion >= 0.9 && !is_in_intersection_) {
            if (!curr_destination_->addVehicleToQueue(id_, stop_token)) {
                return; // stop requested while waiting
            }
            is_in_intersection_ = true;
        }

        if (completion >= 1.0 && is_in_intersection_) {
            moveToNextStreet();
        }

        // Reset after any blocking above so waiting time isn't counted as
        // driving time.
        last_update = std::chrono::steady_clock::now();
    }
}

void Vehicle::moveToNextStreet() {
    auto street_options = curr_destination_->queryStreets(curr_street_);

    std::shared_ptr<Street> next_street;
    if (street_options.empty()) {
        next_street = curr_street_; // dead end — turn around
    } else {
        std::uniform_int_distribution<std::size_t> dist(
            0, street_options.size() - 1);
        next_street = street_options.at(dist(rng_));
    }
    auto next_destination = next_street->getOtherEnd(curr_destination_);

    curr_destination_->vehicleHasLeft();
    setCurrentDestination(std::move(next_destination));
    setCurrentStreet(std::move(next_street));
    is_in_intersection_ = false;
}
