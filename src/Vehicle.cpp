#include "Vehicle.h"
#include "Intersection.h"
#include "Street.h"

#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>

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
    threads_.emplace_back(
        std::jthread([this](std::stop_token st) { drive(st); }));
}

void Vehicle::drive(std::stop_token stop_token) {
    {
        std::lock_guard<std::mutex> lock(cout_mtx_);
        std::cout << "Vehicle #" << id_
                  << "::drive: thread id = " << std::this_thread::get_id()
                  << "\n";
    }

    auto last_update = std::chrono::steady_clock::now();

    while (!stop_token.stop_requested()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        const auto now = std::chrono::steady_clock::now();
        const double dt_seconds =
            std::chrono::duration<double>(now - last_update).count();

        // Use base speed directly; slow down by factor 10 when in intersection
        const double effective_speed =
            is_in_intersection_ ? base_speed_ / 10.0 : base_speed_;

        pos_street_ += effective_speed * dt_seconds;

        // Compute completion fraction along current street
        const double completion = pos_street_ / curr_street_->getLength();

        // Determine start and end intersections for this direction of travel
        std::shared_ptr<Intersection> i_start;
        std::shared_ptr<Intersection> i_end = curr_destination_;

        if (i_end->getID() == curr_street_->getInIntersection()->getID()) {
            i_start = curr_street_->getOutIntersection();
        } else {
            i_start = curr_street_->getInIntersection();
        }

        // Interpolate pixel position along the street
        double x1, y1, x2, y2;
        i_start->getPosition(x1, y1);
        i_end->getPosition(x2, y2);

        const double xv = x1 + completion * (x2 - x1);
        const double yv = y1 + completion * (y2 - y1);
        setPosition(xv, yv);

        // Approaching destination — request entry. Called directly on this
        // thread so it can be woken by our stop_token at shutdown.
        if (completion >= 0.9 && !is_in_intersection_) {
            if (!curr_destination_->addVehicleToQueue(id_, stop_token)) {
                return; // stop requested while waiting
            }
            is_in_intersection_ = true;
        }

        // Crossed the intersection — pick next street
        if (completion >= 1.0 && is_in_intersection_) {
            auto street_options =
                curr_destination_->queryStreets(curr_street_);

            std::shared_ptr<Street> next_street;
            if (!street_options.empty()) {
                std::uniform_int_distribution<std::size_t> dist(
                    0, street_options.size() - 1);
                next_street = street_options.at(dist(rng_));
            } else {
                // Dead end — reverse direction
                next_street = curr_street_;
            }

            // Pick the intersection at the other end of the chosen street
            std::shared_ptr<Intersection> next_intersection =
                (next_street->getInIntersection()->getID() ==
                 curr_destination_->getID())
                    ? next_street->getOutIntersection()
                    : next_street->getInIntersection();

            // Signal that we've left the current intersection
            curr_destination_->vehicleHasLeft();

            // Move to new street
            setCurrentDestination(std::move(next_intersection));
            setCurrentStreet(std::move(next_street));

            is_in_intersection_ = false;
        }

        // Reset after any blocking above so waiting time isn't counted as
        // driving time.
        last_update = std::chrono::steady_clock::now();
    }
}
