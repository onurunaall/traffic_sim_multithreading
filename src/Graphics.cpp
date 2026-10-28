#include "Graphics.h"
#include "Intersection.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

bool Graphics::simulate() {
    if (!loadBackgroundImg()) {
        return false;
    }

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        drawTrafficObjects();

        // Check if user closed the window (waitKey returns -1 if no key,
        // but getWindowProperty returns < 0 if window was closed)
        if (cv::getWindowProperty(window_name_, cv::WND_PROP_VISIBLE) < 1.0) {
            break;
        }
    }

    cv::destroyAllWindows();
    return true;
}

bool Graphics::loadBackgroundImg() {
    cv::Mat background = cv::imread(bg_filename_);
    if (background.empty()) {
        std::cerr << "Error: could not load background image: " << bg_filename_
                  << "\n";
        return false;
    }

    // Only open the window once there is something to show; otherwise the
    // render loop would never pump window events.
    window_name_ = "Concurrent Traffic Simulation";
    cv::namedWindow(window_name_, cv::WINDOW_NORMAL);

    images_.push_back(background);         // [0] original background
    images_.push_back(background.clone()); // [1] overlay layer
    images_.push_back(background.clone()); // [2] composited output
    return true;
}

void Graphics::drawTrafficObjects() {
    if (images_.size() < 3) return;

    // Reset overlay and output to background
    images_.at(0).copyTo(images_.at(1));
    images_.at(0).copyTo(images_.at(2));

    for (const auto &obj : traffic_objects_) {
        double pos_x, pos_y;
        obj->getPosition(pos_x, pos_y);

        if (obj->getType() == ObjectType::kIntersection) {
            auto intersection = std::dynamic_pointer_cast<Intersection>(obj);
            cv::Scalar color = intersection->trafficLightIsGreen()
                                   ? cv::Scalar(0, 255, 0)   // green
                                   : cv::Scalar(0, 0, 255);  // red
            cv::circle(images_.at(1),
                       cv::Point2d(pos_x, pos_y), 25, color, -1);
        } else if (obj->getType() == ObjectType::kVehicle) {
            // Deterministic color per vehicle ID
            cv::RNG rng(obj->getID());
            int b = rng.uniform(0, 255);
            int g = rng.uniform(0, 255);
            int r = static_cast<int>(
                std::sqrt(std::max(0, 255 * 255 - g * g - b * b)));
            cv::circle(images_.at(1),
                       cv::Point2d(pos_x, pos_y), 50,
                       cv::Scalar(b, g, r), -1);
        }
    }

    constexpr float kOpacity = 0.85f;
    cv::addWeighted(images_.at(1), kOpacity,
                    images_.at(0), 1.0 - kOpacity, 0, images_.at(2));

    // Fit into 1040x720 while keeping the image's aspect ratio
    const double scale = std::min(1040.0 / images_.at(2).cols,
                                  720.0 / images_.at(2).rows);
    cv::Mat display;
    cv::resize(images_.at(2), display, cv::Size(), scale, scale, cv::INTER_LINEAR);
    cv::imshow(window_name_, display);
    cv::waitKey(33);
}
