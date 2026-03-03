#ifndef TRAFFIC_LIGHT_H
#define TRAFFIC_LIGHT_H

#include "MessageQueue.h"
#include "TrafficObject.h"

enum class TrafficLightPhase {
    kRed,
    kGreen,
};

class TrafficLight : public TrafficObject {
  public:
    TrafficLight();

    [[nodiscard]] TrafficLightPhase getCurrentPhase() const;

    /// Blocks the calling thread until the traffic light turns green.
    void waitForGreen();

    void simulate() override;

  private:
    /// Infinite loop that toggles the phase at random intervals (4–6 s).
    /// Respects the stop_token for graceful shutdown.
    void cycleThroughPhases(std::stop_token stop_token);

    std::atomic<TrafficLightPhase> current_phase_;
    MessageQueue<TrafficLightPhase> phase_queue_;
};

#endif // TRAFFIC_LIGHT_H
