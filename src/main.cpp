#include "Graphics.h"
#include "Intersection.h"
#include "Street.h"
#include "Vehicle.h"

#include <algorithm>
#include <functional>
#include <initializer_list>
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

struct Position {
    double x;
    double y;
};

using StreetEnd = std::shared_ptr<Intersection> (Street::*)() const;

void addIntersections(TrafficScene &scene,
                      std::initializer_list<Position> positions) {
    for (const auto &[x, y] : positions) {
        auto intersection = std::make_shared<Intersection>();
        intersection->setPosition(x, y);
        scene.intersections.push_back(std::move(intersection));
    }
}

void connect(TrafficScene &scene, std::size_t in_idx, std::size_t out_idx) {
    auto street = std::make_shared<Street>();
    street->setInIntersection(scene.intersections[in_idx]);
    street->setOutIntersection(scene.intersections[out_idx]);
    scene.streets.push_back(std::move(street));
}

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

void addVehicles(TrafficScene &scene, int requested, StreetEnd destination) {
    const std::size_t count = clampVehicleCount(requested, scene.streets.size());
    for (std::size_t i = 0; i < count; ++i) {
        auto vehicle = std::make_shared<Vehicle>();
        vehicle->setCurrentStreet(scene.streets[i]);
        vehicle->setCurrentDestination(std::invoke(destination, scene.streets[i]));
        scene.vehicles.push_back(std::move(vehicle));
    }
}

TrafficScene createParisScene(int num_vehicles) {
    TrafficScene scene;
    scene.background_image = PROJECT_DATA_DIR "/paris.jpg";

    // Pixel coordinates, clockwise on screen, then the central plaza
    addIntersections(scene, {{385, 270},
                             {1240, 80},
                             {1625, 75},
                             {2110, 75},
                             {2840, 175},
                             {3070, 680},
                             {2800, 1400},
                             {400, 1100},
                             {1700, 900}});

    // All outer intersections connect to the central plaza
    constexpr std::size_t kCentralPlaza = 8;
    for (std::size_t i = 0; i < kCentralPlaza; ++i) {
        connect(scene, i, kCentralPlaza);
    }

    addVehicles(scene, num_vehicles, &Street::getOutIntersection);
    return scene;
}

TrafficScene createNYCScene(int num_vehicles) {
    TrafficScene scene;
    scene.background_image = PROJECT_DATA_DIR "/nyc.jpg";

    addIntersections(scene, {{1430, 625},
                             {2575, 1260},
                             {2200, 1950},
                             {1000, 1350},
                             {400, 1000},
                             {750, 250}});

    // Ring topology with one diagonal
    connect(scene, 0, 1);
    connect(scene, 1, 2);
    connect(scene, 2, 3);
    connect(scene, 3, 4);
    connect(scene, 4, 5);
    connect(scene, 5, 0);
    connect(scene, 0, 3); // diagonal

    addVehicles(scene, num_vehicles, &Street::getInIntersection);
    return scene;
}

int main() {
    constexpr int kNumVehicles = 6;
    TrafficScene scene = createParisScene(kNumVehicles);

    for (auto &intersection : scene.intersections) {
        intersection->simulate();
    }
    for (auto &vehicle : scene.vehicles) {
        vehicle->simulate();
    }

    std::vector<std::shared_ptr<TrafficObject>> all_objects(
        scene.intersections.begin(), scene.intersections.end());
    all_objects.insert(all_objects.end(), scene.vehicles.begin(),
                       scene.vehicles.end());

    Graphics graphics;
    graphics.setBgFilename(scene.background_image);
    graphics.setTrafficObjects(std::move(all_objects));
    return graphics.simulate() ? 0 : 1;
}
