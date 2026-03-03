#ifndef INTERSECTION_H
#define INTERSECTION_H

#include "TrafficLight.h"
#include "TrafficObject.h"

#include <atomic>
#include <deque>
#include <future>
#include <memory>
#include <mutex>
#include <vector>

// Forward declarations
class Street;
class Vehicle;

/// Thread-safe FIFO queue for vehicles waiting to enter an intersection.
class WaitingVehicles {
  public:
    [[nodiscard]] int getSize();
    void pushBack(std::shared_ptr<Vehicle> vehicle, std::promise<void> &&promise);
    void permitEntryToFirstInQueue();

  private:
    std::deque<std::shared_ptr<Vehicle>> vehicles_;
    std::deque<std::promise<void>> promises_;
    std::mutex mtx_;
};

class Intersection : public TrafficObject {
  public:
    Intersection();

    void setIsBlocked(bool blocked);

    void addStreet(std::shared_ptr<Street> street);
    [[nodiscard]] std::vector<std::shared_ptr<Street>>
    queryStreets(const std::shared_ptr<Street> &incoming) const;

    void addVehicleToQueue(std::shared_ptr<Vehicle> vehicle);
    void vehicleHasLeft(std::shared_ptr<Vehicle> vehicle);

    [[nodiscard]] bool trafficLightIsGreen() const;

    void simulate() override;

  private:
    void processVehicleQueue(std::stop_token stop_token);

    std::vector<std::shared_ptr<Street>> streets_;
    WaitingVehicles waiting_vehicles_;
    std::atomic<bool> is_blocked_{false};
    TrafficLight traffic_light_;
};

#endif // INTERSECTION_H
