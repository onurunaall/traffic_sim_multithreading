#include "Street.h"
#include "Intersection.h"

Street::Street() : length_(1000.0) {
    type_ = ObjectType::kStreet;
}

void Street::setInIntersection(std::shared_ptr<Intersection> in) {
    inter_in_ = in;
    in->addStreet(shared_from_this());
}

void Street::setOutIntersection(std::shared_ptr<Intersection> out) {
    inter_out_ = out;
    out->addStreet(shared_from_this());
}
