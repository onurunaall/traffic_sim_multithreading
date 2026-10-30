#ifndef INTERSECTION_H
#define INTERSECTION_H

#include "TrafficLight.h"
#include "TrafficObject.h"

#include <atomic>
#include <deque>
#include <future>
#include <memory>
#include <mutex>
#include <stop_token>
#include <vector>

// Forward declaration
class Street;

/// Thread-safe FIFO queue for vehicles waiting to enter an intersection.
/// Stores vehicle IDs rather than shared_ptrs so a queued vehicle is never
/// kept alive by the queue.
class WaitingVehicles {
  public:
    [[nodiscard]] int getSize();
    void pushBack(int vehicle_id, std::promise<void> &&promise);

    /// Fulfills the first vehicle's promise. Returns false if the queue was empty.
    bool permitEntryToFirstInQueue();

    /// Drops a vehicle without granting entry. Destroying its promise makes
    /// the waiting future ready (with broken_promise), which unblocks it.
    void remove(int vehicle_id);

  private:
    struct Entry {
        int vehicle_id;
        std::promise<void> promise;
    };

    std::deque<Entry> entries_;
    std::mutex mtx_;
};

class Intersection : public TrafficObject {
  public:
    Intersection();
    ~Intersection() override;

    void setIsBlocked(bool blocked);

    void addStreet(const std::shared_ptr<Street> &street);
    [[nodiscard]] std::vector<std::shared_ptr<Street>>
    queryStreets(const std::shared_ptr<Street> &incoming) const;

    /// Blocks until the vehicle is granted entry and the light is green.
    /// Returns false if stop was requested on stop_token before that.
    bool addVehicleToQueue(int vehicle_id, std::stop_token stop_token = {});
    void vehicleHasLeft();

    [[nodiscard]] bool trafficLightIsGreen() const;

    /// Number of vehicles that have crossed this intersection so far.
    [[nodiscard]] int getNumCrossings() const { return num_crossings_.load(); }

    void simulate() override;

  private:
    void processVehicleQueue(std::stop_token stop_token);

    // Non-owning: streets own their intersections, so owning the streets here
    // too would create a shared_ptr cycle that is never freed.
    std::vector<std::weak_ptr<Street>> streets_;
    WaitingVehicles waiting_vehicles_;
    std::atomic<bool> is_blocked_{false};
    std::atomic<int> num_crossings_{0};
    TrafficLight traffic_light_;
};

#endif // INTERSECTION_H
