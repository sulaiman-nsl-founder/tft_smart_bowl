#ifndef SERVICES_STABILITY_DETECTOR_H
#define SERVICES_STABILITY_DETECTOR_H

#include <stdint.h>

namespace Services {

/**
 * @brief Detects when the weight reading has stabilized.
 *
 * Uses a sliding window of recent samples. If the difference between the
 * maximum and minimum values in the window is below a defined threshold
 * for a specific duration, the weight is considered stable.
 */
class StabilityDetector {
public:
  static constexpr uint8_t WINDOW_SIZE = 10;

  StabilityDetector();

  /**
   * @brief Reset the detector.
   */
  void reset();

  /**
   * @brief Add a new filtered sample to the stability window.
   * @param sample The new sample.
   * @return true if the weight is currently stable.
   */
  bool addSample(int32_t sample);

  /**
   * @brief Check current stability status.
   * @return true if stable.
   */
  bool isStable() const;

private:
  int32_t _window[WINDOW_SIZE];
  uint8_t _index;
  uint8_t _count;
  bool _isStable;

  // Threshold of variation allowed within the window (in raw ADC units)
  static constexpr int32_t STABILITY_THRESHOLD = 5000;
};

} // namespace Services

#endif // SERVICES_STABILITY_DETECTOR_H
