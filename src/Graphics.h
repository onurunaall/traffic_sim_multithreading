#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "TrafficObject.h"

#include <memory>
#include <string>
#include <vector>

#include <opencv2/core.hpp>

class Graphics {
  public:
    void setBgFilename(std::string filename) { bg_filename_ = std::move(filename); }

    void setTrafficObjects(
        std::vector<std::shared_ptr<TrafficObject>> traffic_objects) {
        traffic_objects_ = std::move(traffic_objects);
    }

    /// Runs the rendering loop. Blocks until the user closes the window.
    /// Returns false if the background image could not be loaded.
    bool simulate();

  private:
    bool loadBackgroundImg();
    void drawTrafficObjects();

    std::vector<std::shared_ptr<TrafficObject>> traffic_objects_;
    std::string bg_filename_;
    cv::Mat background_;
    cv::Mat overlay_;
};

#endif // GRAPHICS_H
