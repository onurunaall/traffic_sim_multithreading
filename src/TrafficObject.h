#ifndef TRAFFIC_OBJECT_H
#define TRAFFIC_OBJECT_H

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

enum class ObjectType {kNone, kVehicle, kIntersection, kStreet};

class TrafficObject {
  public:
    TrafficObject();
    virtual ~TrafficObject();

    // Non-copyable, non-movable (owns threads)
    TrafficObject(const TrafficObject &) = delete;
    TrafficObject &operator=(const TrafficObject &) = delete;
    TrafficObject(TrafficObject &&) = delete;
    TrafficObject &operator=(TrafficObject &&) = delete;

    [[nodiscard]] int getID() const { return id_; }
    [[nodiscard]] ObjectType getType() const { return type_; }

    void setPosition(double x, double y);
    void getPosition(double &x, double &y) const;

    virtual void simulate() {};

  protected:
    ObjectType type_{ObjectType::kNone};
    int id_;
    std::atomic<double> pos_x_{0.0};
    std::atomic<double> pos_y_{0.0};

    std::vector<std::jthread> threads_;

    // Shared mutex for synchronized console output only.
    static std::mutex cout_mtx_;

  private:
    static inline std::atomic<int> id_counter_{0};
};

#endif // TRAFFIC_OBJECT_H
