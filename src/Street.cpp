#include "Street.h"
#include "Intersection.h"

#include <cmath>
#include <utility>

Street::Street() {
    type_ = ObjectType::kStreet;
}

double Street::getLength() const {
    double x1 = 0.0, y1 = 0.0, x2 = 0.0, y2 = 0.0;
    inter_in_->getPosition(x1, y1);
    inter_out_->getPosition(x2, y2);
    return std::hypot(x2 - x1, y2 - y1);
}

void Street::setInIntersection(std::shared_ptr<Intersection> in) {
    inter_in_ = std::move(in);
    inter_in_->addStreet(shared_from_this());
}

void Street::setOutIntersection(std::shared_ptr<Intersection> out) {
    inter_out_ = std::move(out);
    inter_out_->addStreet(shared_from_this());
}
