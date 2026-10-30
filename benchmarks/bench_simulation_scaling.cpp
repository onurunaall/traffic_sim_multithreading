#include "Intersection.h"
#include "Street.h"
#include "Vehicle.h"

#include <benchmark/benchmark.h>

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

/// Build a ring topology of N intersections with N streets connecting them.
/// Place `num_vehicles` vehicles distributed across the streets.
///
/// This isolates the concurrency scaling behavior: more vehicles means more
/// contention at intersections.
struct BenchTopology {
    std::vector<std::shared_ptr<Intersection>> intersections;
    std::vector<std::shared_ptr<Street>> streets;
    // Declared last so vehicles are destroyed (and joined) first.
    std::vector<std::shared_ptr<Vehicle>> vehicles;

    void start() {
        for (auto &i : intersections) i->simulate();
        for (auto &v : vehicles)      v->simulate();
    }

    [[nodiscard]] int totalCrossings() const {
        int total = 0;
        for (const auto &i : intersections) total += i->getNumCrossings();
        return total;
    }

    // Destruction stops and joins all threads via jthread/stop_token.
};

static BenchTopology buildRing(int num_intersections, int num_vehicles) {
    BenchTopology topo;

    // Create intersections arranged in a line, 100 px apart. Streets get
    // their length from these positions; the topology is what matters.
    topo.intersections.reserve(static_cast<std::size_t>(num_intersections));
    for (int i = 0; i < num_intersections; ++i) {
        auto inter = std::make_shared<Intersection>();
        inter->setPosition(
            static_cast<double>(i) * 100.0,
            0.0);
        topo.intersections.push_back(std::move(inter));
    }

    // Connect in a ring: 0→1, 1→2, ..., (N-1)→0
    topo.streets.reserve(static_cast<std::size_t>(num_intersections));
    for (int i = 0; i < num_intersections; ++i) {
        auto street = std::make_shared<Street>();
        street->setInIntersection(
            topo.intersections[static_cast<std::size_t>(i)]);
        street->setOutIntersection(
            topo.intersections[static_cast<std::size_t>(
                (i + 1) % num_intersections)]);
        topo.streets.push_back(std::move(street));
    }

    // Distribute vehicles across streets
    topo.vehicles.reserve(static_cast<std::size_t>(num_vehicles));
    for (int i = 0; i < num_vehicles; ++i) {
        auto vehicle = std::make_shared<Vehicle>();
        auto street_idx = static_cast<std::size_t>(
            i % num_intersections);
        vehicle->setCurrentStreet(topo.streets[street_idx]);
        vehicle->setCurrentDestination(
            topo.streets[street_idx]->getOutIntersection());
        topo.vehicles.push_back(std::move(vehicle));
    }

    return topo;
}

/// Runs the topology for a fixed wall-clock duration and reports how many
/// intersection crossings occurred — the key throughput metric for the
/// concurrency model.
static void runAndCountCrossings(benchmark::State &state,
                                 int num_intersections, int num_vehicles) {
    constexpr auto kRunDuration = std::chrono::seconds(3);
    int crossings = 0;

    for (auto _ : state) {
        state.PauseTiming();
        auto topo = buildRing(num_intersections, num_vehicles);
        state.ResumeTiming();

        topo.start();

        // Let the simulation run for a fixed duration
        std::this_thread::sleep_for(kRunDuration);
        crossings = topo.totalCrossings();

        // Topology destructor stops all threads
    }

    state.counters["vehicles"] = benchmark::Counter(
        static_cast<double>(num_vehicles));
    state.counters["intersections"] = benchmark::Counter(
        static_cast<double>(num_intersections));
    state.counters["crossings"] = benchmark::Counter(
        static_cast<double>(crossings));
    state.counters["crossings_per_s"] = benchmark::Counter(
        static_cast<double>(crossings) /
        std::chrono::duration<double>(kRunDuration).count());
}

/// Benchmark: measure how the simulation scales with vehicle count.
///
/// Range: number of vehicles (1, 2, 4, 8, 16, 32, 64)
///
/// The number of intersections is fixed at 8 to keep the topology constant
/// and isolate the effect of vehicle-count contention.
static void BM_SimulationScaling(benchmark::State &state) {
    const int num_vehicles = static_cast<int>(state.range(0));
    constexpr int kNumIntersections = 8;

    runAndCountCrossings(state, kNumIntersections, num_vehicles);
    state.SetLabel(std::to_string(num_vehicles) + " vehicles");
}
BENCHMARK(BM_SimulationScaling)
    ->Arg(1)->Arg(2)->Arg(4)->Arg(8)->Arg(16)->Arg(32)->Arg(64)
    ->Unit(benchmark::kSecond)
    ->Iterations(1)    // single iteration per config (time-bounded)
    ->UseRealTime();

/// Benchmark: fixed vehicles, scaling intersections.
/// Measures throughput as the road network grows.
static void BM_TopologyScaling(benchmark::State &state) {
    const int num_intersections = static_cast<int>(state.range(0));
    constexpr int kNumVehicles = 8;

    runAndCountCrossings(state, num_intersections, kNumVehicles);
    state.SetLabel(std::to_string(num_intersections) + " intersections");
}
BENCHMARK(BM_TopologyScaling)
    ->Arg(4)->Arg(8)->Arg(16)->Arg(32)
    ->Unit(benchmark::kSecond)
    ->Iterations(1)
    ->UseRealTime();

/// Microbenchmark: one vehicle repeatedly entering and leaving an
/// intersection. Measures queue + promise/future overhead per crossing, plus
/// the time spent waiting whenever the traffic light is red.
static void BM_IntersectionAdmission(benchmark::State &state) {
    // Build minimal topology: 2 intersections, 2 streets (bidirectional)
    auto i1 = std::make_shared<Intersection>();
    auto i2 = std::make_shared<Intersection>();
    i1->setPosition(0, 0);
    i2->setPosition(100, 0);

    auto s1 = std::make_shared<Street>();
    s1->setInIntersection(i1);
    s1->setOutIntersection(i2);

    auto s2 = std::make_shared<Street>();
    s2->setInIntersection(i2);
    s2->setOutIntersection(i1);

    i1->simulate();

    auto vehicle = std::make_shared<Vehicle>();

    for (auto _ : state) {
        // Reset blocked state to allow admission
        i1->setIsBlocked(false);
        i1->addVehicleToQueue(vehicle->getID());
        i1->vehicleHasLeft();
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_IntersectionAdmission)
    ->Unit(benchmark::kMillisecond)
    ->Iterations(50);  // limited iterations because red lights add waits
