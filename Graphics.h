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
        std::vector<std::shared_ptr<TrafficObject>> &traffic_objects) {
        traffic_objects_ = traffic_objects;
    }

    /// Runs the rendering loop. Blocks until the user closes the window.
    /// Returns true if the window was closed normally.
    bool simulate();

  private:
    void loadBackgroundImg();
    void drawTrafficObjects();

    std::vector<std::shared_ptr<TrafficObject>> traffic_objects_;
    std::string bg_filename_;
    std::string window_name_;
    std::vector<cv::Mat> images_;
};

#endif // GRAPHICS_H
