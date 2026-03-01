#include "TrafficObject.h"

std::mutex TrafficObject::cout_mtx_;


TrafficObject::TrafficObject() : id_(id_counter_.fetch_add(1)) {}


TrafficObject::~TrafficObject() {
    // Request stop on all jthreads
    for (auto &t : threads_) {
        t.request_stop();
    }

    // jthread destructor calls request_stop() + join(), so no manual join needed.
    threads_.clear();
}


void TrafficObject::setPosition(double x, double y) {
    pos_x_.store(x, std::memory_order_relaxed);
    pos_y_.store(y, std::memory_order_relaxed);
}


void TrafficObject::getPosition(double &x, double &y) const {
    x = pos_x_.load(std::memory_order_relaxed);
    y = pos_y_.load(std::memory_order_relaxed);
}
