#ifndef STREET_H
#define STREET_H

#include "TrafficObject.h"

#include <memory>

// Forward declaration
class Intersection;

class Street : public TrafficObject, public std::enable_shared_from_this<Street> {
  public:
    Street();

    [[nodiscard]] double getLength() const { return length_; }

    void setInIntersection(std::shared_ptr<Intersection> in);
    void setOutIntersection(std::shared_ptr<Intersection> out);

    [[nodiscard]] std::shared_ptr<Intersection> getInIntersection() const {
        return inter_in_;
    }
    [[nodiscard]] std::shared_ptr<Intersection> getOutIntersection() const {
        return inter_out_;
    }

  private:
    double length_;
    std::shared_ptr<Intersection> inter_in_;
    std::shared_ptr<Intersection> inter_out_;
};

#endif // STREET_H
