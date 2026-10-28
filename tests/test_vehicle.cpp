#include "Intersection.h"
#include "Street.h"
#include "Vehicle.h"

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <thread>

TEST(VehicleTest, TypeIsVehicle) {
    Vehicle v;
    EXPECT_EQ(v.getType(), ObjectType::kVehicle);
}

TEST(VehicleTest, UniqueIDs) {
    Vehicle v1, v2, v3;
    EXPECT_NE(v1.getID(), v2.getID());
    EXPECT_NE(v2.getID(), v3.getID());
    EXPECT_NE(v1.getID(), v3.getID());
}

TEST(VehicleTest, PositionDefaultsToZero) {
    Vehicle v;
    double x, y;
    v.getPosition(x, y);
    EXPECT_DOUBLE_EQ(x, 0.0);
    EXPECT_DOUBLE_EQ(y, 0.0);
}

TEST(VehicleTest, SetAndGetPosition) {
    Vehicle v;
    v.setPosition(42.5, -13.7);
    double x, y;
    v.getPosition(x, y);
    EXPECT_DOUBLE_EQ(x, 42.5);
    EXPECT_DOUBLE_EQ(y, -13.7);
}

TEST(VehicleTest, DestructionWhileWaitingAtIntersectionDoesNotHang) {
    // 100 px street at 400 px/s: the vehicle reaches the queue in ~0.25 s.
    // The destination is never simulated, so the vehicle waits there forever
    // unless destruction wakes it up.
    auto start_inter = std::make_shared<Intersection>();
    auto destination = std::make_shared<Intersection>();
    start_inter->setPosition(0, 0);
    destination->setPosition(100, 0);

    auto street = std::make_shared<Street>();
    street->setInIntersection(start_inter);
    street->setOutIntersection(destination);

    auto vehicle = std::make_shared<Vehicle>();
    vehicle->setCurrentStreet(street);
    vehicle->setCurrentDestination(destination);
    vehicle->simulate();

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    auto start = std::chrono::steady_clock::now();
    vehicle.reset();  // ~Vehicle stops and joins the drive thread
    EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(1));
}
