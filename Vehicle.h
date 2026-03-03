#ifndef VEHICLE_H
#define VEHICLE_H

#include "TrafficObject.h"

#include <memory>
#include <random>

// Forward declarations
class Street;
class Intersection;

class Vehicle : public TrafficObject, public std::enable_shared_from_this<Vehicle> {
  public:
    Vehicle();

    void setCurrentStreet(std::shared_ptr<Street> street) {
        curr_street_ = std::move(street);
    }
    void setCurrentDestination(std::shared_ptr<Intersection> destination);

    void simulate() override;

  private:
    void drive(std::stop_token stop_token);

    std::shared_ptr<Street> curr_street_;
    std::shared_ptr<Intersection> curr_destination_;
    double pos_street_{0.0};      // progress along current street [0..length]
    double base_speed_{400.0};    // base ego speed in m/s
    bool is_in_intersection_{false};

    std::mt19937 rng_;            // per-vehicle RNG, seeded once
};

#endif // VEHICLE_H
