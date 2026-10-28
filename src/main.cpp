#include "Graphics.h"
#include "Intersection.h"
#include "Street.h"
#include "Vehicle.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <vector>


// City map builders
struct TrafficScene {
    std::vector<std::shared_ptr<Street>> streets;
    std::vector<std::shared_ptr<Intersection>> intersections;
    // Declared last so vehicles are destroyed (and their threads joined)
    // first, while the streets and intersections they use still exist.
    std::vector<std::shared_ptr<Vehicle>> vehicles;
    std::string background_image;
};


// Each vehicle starts on its own street, so at most one vehicle per street.
std::size_t clampVehicleCount(int requested, std::size_t num_streets) {
    const auto count = static_cast<std::size_t>(std::max(requested, 0));
    if (count > num_streets) {
        std::cerr << "Warning: " << requested << " vehicles requested but the "
                  << "scene has only " << num_streets << " streets; placing "
                  << num_streets << ".\n";
        return num_streets;
    }
    return count;
}


TrafficScene createParisScene(int num_vehicles) {
    TrafficScene scene;
    scene.background_image = PROJECT_DATA_DIR "/paris.jpg";

    constexpr int kNumIntersections = 9;
    scene.intersections.reserve(kNumIntersections);
    for (int i = 0; i < kNumIntersections; ++i) {
        scene.intersections.push_back(std::make_shared<Intersection>());
    }

    // Position intersections in pixel coordinates (clockwise on screen)
    scene.intersections[0]->setPosition(385, 270);
    scene.intersections[1]->setPosition(1240, 80);
    scene.intersections[2]->setPosition(1625, 75);
    scene.intersections[3]->setPosition(2110, 75);
    scene.intersections[4]->setPosition(2840, 175);
    scene.intersections[5]->setPosition(3070, 680);
    scene.intersections[6]->setPosition(2800, 1400);
    scene.intersections[7]->setPosition(400, 1100);
    scene.intersections[8]->setPosition(1700, 900); // central plaza

    // All outer intersections connect to central plaza (index 8)
    constexpr int kNumStreets = 8;
    scene.streets.reserve(kNumStreets);
    for (int i = 0; i < kNumStreets; ++i) {
        auto street = std::make_shared<Street>();
        street->setInIntersection(scene.intersections[static_cast<std::size_t>(i)]);
        street->setOutIntersection(scene.intersections[8]);
        scene.streets.push_back(std::move(street));
    }

    // Place vehicles on streets heading toward central plaza
    const std::size_t vehicle_count =
        clampVehicleCount(num_vehicles, scene.streets.size());
    scene.vehicles.reserve(vehicle_count);
    for (std::size_t i = 0; i < vehicle_count; ++i) {
        auto vehicle = std::make_shared<Vehicle>();
        vehicle->setCurrentStreet(scene.streets[i]);
        vehicle->setCurrentDestination(scene.intersections[8]);
        scene.vehicles.push_back(std::move(vehicle));
    }

    return scene;
}


TrafficScene createNYCScene(int num_vehicles) {
    TrafficScene scene;
    scene.background_image = PROJECT_DATA_DIR "/nyc.jpg";

    constexpr int kNumIntersections = 6;
    scene.intersections.reserve(kNumIntersections);
    for (int i = 0; i < kNumIntersections; ++i) {
        scene.intersections.push_back(std::make_shared<Intersection>());
    }

    scene.intersections[0]->setPosition(1430, 625);
    scene.intersections[1]->setPosition(2575, 1260);
    scene.intersections[2]->setPosition(2200, 1950);
    scene.intersections[3]->setPosition(1000, 1350);
    scene.intersections[4]->setPosition(400, 1000);
    scene.intersections[5]->setPosition(750, 250);

    // Ring topology with one diagonal
    auto connect = [&](int street_idx, int in_idx, int out_idx) {
        auto street = std::make_shared<Street>();
        street->setInIntersection(
            scene.intersections[static_cast<std::size_t>(in_idx)]);
        street->setOutIntersection(
            scene.intersections[static_cast<std::size_t>(out_idx)]);
        if (static_cast<std::size_t>(street_idx) >= scene.streets.size()) {
            scene.streets.resize(static_cast<std::size_t>(street_idx) + 1);
        }
        scene.streets[static_cast<std::size_t>(street_idx)] = std::move(street);
    };

    connect(0, 0, 1);
    connect(1, 1, 2);
    connect(2, 2, 3);
    connect(3, 3, 4);
    connect(4, 4, 5);
    connect(5, 5, 0);
    connect(6, 0, 3); // diagonal

    // Each vehicle drives toward its street's in-intersection
    const std::size_t vehicle_count =
        clampVehicleCount(num_vehicles, scene.streets.size());
    scene.vehicles.reserve(vehicle_count);
    for (std::size_t i = 0; i < vehicle_count; ++i) {
        auto vehicle = std::make_shared<Vehicle>();
        vehicle->setCurrentStreet(scene.streets[i]);
        vehicle->setCurrentDestination(scene.streets[i]->getInIntersection());
        scene.vehicles.push_back(std::move(vehicle));
    }

    return scene;
}


int main() {
    // Build traffic scene
    constexpr int kNumVehicles = 6;
    TrafficScene scene = createParisScene(kNumVehicles);

    // Start simulation threads
    for (auto &intersection : scene.intersections) {
        intersection->simulate();
    }
    for (auto &vehicle : scene.vehicles) {
        vehicle->simulate();
    }

    // Collect all objects for rendering (intersections + vehicles)
    std::vector<std::shared_ptr<TrafficObject>> all_objects;
    all_objects.reserve(scene.intersections.size() + scene.vehicles.size());

    for (auto &i : scene.intersections) {
        all_objects.push_back(i);
    }
    for (auto &v : scene.vehicles) {
        all_objects.push_back(v);
    }

    // Run visualization
    Graphics graphics;
    graphics.setBgFilename(scene.background_image);
    graphics.setTrafficObjects(all_objects);
    return graphics.simulate() ? 0 : 1;
}
