#include "Intersection.h"
#include "Street.h"
#include "Vehicle.h"

#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <memory>
#include <thread>

// ─── Helper: build a minimal intersection topology ──────────────────

struct MiniTopology {
    std::shared_ptr<Intersection> center;
    std::shared_ptr<Intersection> north;
    std::shared_ptr<Intersection> south;
    std::shared_ptr<Street> street_n;   // north → center
    std::shared_ptr<Street> street_s;   // center → south
};

MiniTopology buildMiniTopology() {
    MiniTopology t;
    t.center = std::make_shared<Intersection>();
    t.north  = std::make_shared<Intersection>();
    t.south  = std::make_shared<Intersection>();

    t.center->setPosition(100, 100);
    t.north->setPosition(100, 0);
    t.south->setPosition(100, 200);

    t.street_n = std::make_shared<Street>();
    t.street_n->setInIntersection(t.north);
    t.street_n->setOutIntersection(t.center);

    t.street_s = std::make_shared<Street>();
    t.street_s->setInIntersection(t.center);
    t.street_s->setOutIntersection(t.south);

    return t;
}

// ─── Tests ───────────────────────────────────────────────────────────

TEST(IntersectionTest, TypeIsIntersection) {
    Intersection i;
    EXPECT_EQ(i.getType(), ObjectType::kIntersection);
}

TEST(IntersectionTest, QueryStreetsExcludesIncoming) {
    auto topo = buildMiniTopology();

    // From center's perspective, asking "what streets can I go to, excluding
    // street_n?" should return street_s.
    auto outgoing = topo.center->queryStreets(topo.street_n);
    ASSERT_EQ(outgoing.size(), 1u);
    EXPECT_EQ(outgoing[0]->getID(), topo.street_s->getID());
}

TEST(IntersectionTest, QueryStreetsReturnsAllButIncoming) {
    // Build a 3-street intersection
    auto center = std::make_shared<Intersection>();
    auto a = std::make_shared<Intersection>();
    auto b = std::make_shared<Intersection>();
    auto c = std::make_shared<Intersection>();

    auto s1 = std::make_shared<Street>();
    s1->setInIntersection(a);
    s1->setOutIntersection(center);

    auto s2 = std::make_shared<Street>();
    s2->setInIntersection(center);
    s2->setOutIntersection(b);

    auto s3 = std::make_shared<Street>();
    s3->setInIntersection(center);
    s3->setOutIntersection(c);

    auto outgoing = center->queryStreets(s1);
    EXPECT_EQ(outgoing.size(), 2u);
}

TEST(IntersectionTest, TopologyIsFreedWhenReleased) {
    // Streets and intersections must not keep each other alive.
    std::weak_ptr<Intersection> center;
    std::weak_ptr<Street> street;
    {
        auto topo = buildMiniTopology();
        topo.center->simulate();
        center = topo.center;
        street = topo.street_n;
    }
    EXPECT_TRUE(center.expired());
    EXPECT_TRUE(street.expired());
}

TEST(IntersectionTest, TrafficLightStartsRed) {
    Intersection i;
    // Before simulate(), the traffic light should be in its initial state (red)
    EXPECT_FALSE(i.trafficLightIsGreen());
}

TEST(IntersectionTest, TrafficLightEventuallyTurnsGreen) {
    auto topo = buildMiniTopology();
    topo.center->simulate();

    auto start = std::chrono::steady_clock::now();
    while (!topo.center->trafficLightIsGreen()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        if (std::chrono::steady_clock::now() - start > std::chrono::seconds(7)) {
            FAIL() << "Traffic light at intersection never turned green";
        }
    }
    SUCCEED();
}

TEST(IntersectionTest, VehicleCanPassThroughIntersection) {
    // Build topology, start simulation, and verify a vehicle can be admitted.
    auto topo = buildMiniTopology();
    topo.center->simulate();

    auto vehicle = std::make_shared<Vehicle>();
    vehicle->setCurrentStreet(topo.street_n);
    vehicle->setCurrentDestination(topo.center);

    // addVehicleToQueue blocks until (a) queue processes it and (b) light is
    // green. It runs on a jthread so that, if the assertion below fails, the
    // jthread's destructor requests stop and unblocks it instead of hanging.
    std::promise<bool> admitted;
    auto admitted_ftr = admitted.get_future();
    std::jthread worker([&](std::stop_token st) {
        admitted.set_value(
            topo.center->addVehicleToQueue(vehicle->getID(), st));
    });

    // Should complete within a couple of traffic light cycles (~14s max)
    auto status = admitted_ftr.wait_for(std::chrono::seconds(15));
    ASSERT_EQ(status, std::future_status::ready)
        << "Vehicle was never admitted through intersection — possible deadlock";
    EXPECT_TRUE(admitted_ftr.get());

    // Clean up: let the intersection know the vehicle left so it unblocks
    topo.center->vehicleHasLeft();
    EXPECT_EQ(topo.center->getNumCrossings(), 1);
}

TEST(IntersectionTest, StopUnblocksQueuedVehicle) {
    // The intersection is never simulated, so the vehicle is never admitted.
    Intersection intersection;

    std::stop_source stop;
    std::promise<bool> admitted;
    auto admitted_ftr = admitted.get_future();
    std::jthread worker([&] {
        admitted.set_value(intersection.addVehicleToQueue(42, stop.get_token()));
    });

    EXPECT_EQ(admitted_ftr.wait_for(std::chrono::milliseconds(100)),
              std::future_status::timeout);

    stop.request_stop();
    ASSERT_EQ(admitted_ftr.wait_for(std::chrono::seconds(1)),
              std::future_status::ready)
        << "Stop request did not unblock the queued vehicle";
    EXPECT_FALSE(admitted_ftr.get());
}
