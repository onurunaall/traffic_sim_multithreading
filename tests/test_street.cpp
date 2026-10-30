#include "Intersection.h"
#include "Street.h"

#include <gtest/gtest.h>

#include <memory>

TEST(StreetTest, LengthIsDistanceBetweenIntersections) {
    auto in = std::make_shared<Intersection>();
    auto out = std::make_shared<Intersection>();
    in->setPosition(0, 0);
    out->setPosition(300, 400);

    auto street = std::make_shared<Street>();
    street->setInIntersection(in);
    street->setOutIntersection(out);

    EXPECT_DOUBLE_EQ(street->getLength(), 500.0);
}

TEST(StreetTest, ConnectsIntersections) {
    auto in = std::make_shared<Intersection>();
    auto out = std::make_shared<Intersection>();
    auto street = std::make_shared<Street>();

    street->setInIntersection(in);
    street->setOutIntersection(out);

    EXPECT_EQ(street->getInIntersection()->getID(), in->getID());
    EXPECT_EQ(street->getOutIntersection()->getID(), out->getID());
}
