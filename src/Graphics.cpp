#include "Graphics.h"
#include "Intersection.h"

#include <algorithm>
#include <cmath>
#include <iostream>

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

namespace {
const std::string kWindowName = "Concurrent Traffic Simulation";
}

bool Graphics::simulate() {
    if (!loadBackgroundImg()) {
        return false;
    }

    do {
        drawTrafficObjects();
    } while (cv::getWindowProperty(kWindowName, cv::WND_PROP_VISIBLE) >= 1.0);

    cv::destroyAllWindows();
    return true;
}

bool Graphics::loadBackgroundImg() {
    background_ = cv::imread(bg_filename_);
    if (background_.empty()) {
        std::cerr << "Error: could not load background image: " << bg_filename_
                  << "\n";
        return false;
    }

    // Only open the window once there is something to show; otherwise the
    // render loop would never pump window events.
    cv::namedWindow(kWindowName, cv::WINDOW_NORMAL);
    return true;
}

void Graphics::drawTrafficObjects() {
    background_.copyTo(overlay_);

    for (const auto &obj : traffic_objects_) {
        double pos_x, pos_y;
        obj->getPosition(pos_x, pos_y);
        const cv::Point2d position(pos_x, pos_y);

        if (obj->getType() == ObjectType::kIntersection) {
            auto intersection = std::dynamic_pointer_cast<Intersection>(obj);
            cv::Scalar color = intersection->trafficLightIsGreen()
                                   ? cv::Scalar(0, 255, 0)   // green
                                   : cv::Scalar(0, 0, 255);  // red
            cv::circle(overlay_, position, 25, color, -1);
        } else if (obj->getType() == ObjectType::kVehicle) {
            // Deterministic color per vehicle ID
            cv::RNG rng(obj->getID());
            int b = rng.uniform(0, 255);
            int g = rng.uniform(0, 255);
            int r = static_cast<int>(
                std::sqrt(std::max(0, 255 * 255 - g * g - b * b)));
            cv::circle(overlay_, position, 50, cv::Scalar(b, g, r), -1);
        }
    }

    constexpr float kOpacity = 0.85f;
    cv::Mat composite;
    cv::addWeighted(overlay_, kOpacity, background_, 1.0 - kOpacity, 0, composite);

    // Fit into 1040x720 while keeping the image's aspect ratio
    const double scale =
        std::min(1040.0 / composite.cols, 720.0 / composite.rows);
    cv::Mat display;
    cv::resize(composite, display, cv::Size(), scale, scale, cv::INTER_LINEAR);
    cv::imshow(kWindowName, display);
    cv::waitKey(33);
}
