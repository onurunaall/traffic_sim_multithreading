#ifndef STREET_H
#define STREET_H

#include "TrafficObject.h"

#include <memory>

// Forward declaration
class Intersection;

class Street : public TrafficObject, public std::enable_shared_from_this<Street> {
  public:
    Street();

    /// Straight-line distance in pixels between the two intersections.
    /// Both intersections must be set.
    [[nodiscard]] double getLength() const;

    void setInIntersection(std::shared_ptr<Intersection> in);
    void setOutIntersection(std::shared_ptr<Intersection> out);

    [[nodiscard]] std::shared_ptr<Intersection> getInIntersection() const {
        return inter_in_;
    }
    [[nodiscard]] std::shared_ptr<Intersection> getOutIntersection() const {
        return inter_out_;
    }
    [[nodiscard]] std::shared_ptr<Intersection>
    getOtherEnd(const std::shared_ptr<Intersection> &end) const {
        return end == inter_in_ ? inter_out_ : inter_in_;
    }

  private:
    std::shared_ptr<Intersection> inter_in_;
    std::shared_ptr<Intersection> inter_out_;
};

#endif // STREET_H
