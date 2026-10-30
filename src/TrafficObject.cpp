#include "TrafficObject.h"

TrafficObject::TrafficObject() : id_(id_counter_.fetch_add(1)) {}

TrafficObject::~TrafficObject() {
    // No-op if the derived destructor already stopped the threads.
    stopThreads();
}

void TrafficObject::stopThreads() {
    // Request stop on all threads first so they wind down in parallel,
    // then join them.
    for (auto &t : threads_) {
        t.request_stop();
    }
    for (auto &t : threads_) {
        if (t.joinable()) {
            t.join();
        }
    }
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
