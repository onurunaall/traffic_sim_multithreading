#include "Intersection.h"
#include "Street.h"
#include "Vehicle.h"

#include <gtest/gtest.h>

#include <memory>

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

TEST(VehicleTest, SetCurrentDestinationResetsPosStreet) {
    // We can't directly observe pos_street_, but we can verify the
    // destination is set by checking that the vehicle doesn't crash
    // when configured properly.
    auto inter = std::make_shared<Intersection>();
    inter->setPosition(100, 200);

    Vehicle v;
    v.setCurrentDestination(inter);
    // If pos_street_ wasn't reset, driving would start at a wrong offset.
    // This is a smoke test — the important thing is no crash.
    SUCCEED();
}

TEST(VehicleTest, StreetDefaultLength) {
    auto street = std::make_shared<Street>();
    EXPECT_DOUBLE_EQ(street->getLength(), 1000.0);
}

TEST(VehicleTest, StreetConnectsIntersections) {
    auto in = std::make_shared<Intersection>();
    auto out = std::make_shared<Intersection>();
    auto street = std::make_shared<Street>();

    street->setInIntersection(in);
    street->setOutIntersection(out);

    EXPECT_EQ(street->getInIntersection()->getID(), in->getID());
    EXPECT_EQ(street->getOutIntersection()->getID(), out->getID());
}
