#include "services/StabilityDetector.h"

namespace Services {

StabilityDetector::StabilityDetector() { reset(); }

void StabilityDetector::reset() {
  _index = 0;
  _count = 0;
  _isStable = false;
  for (uint8_t i = 0; i < WINDOW_SIZE; i++) {
    _window[i] = 0;
  }
}

bool StabilityDetector::addSample(int32_t sample) {
  _window[_index] = sample;

  _index++;
  if (_index >= WINDOW_SIZE) {
    _index = 0;
  }

  if (_count < WINDOW_SIZE) {
    _count++;
    // Not enough samples to determine stability
    _isStable = false;
    return _isStable;
  }

  // Find min and max in the window
  int32_t minVal = _window[0];
  int32_t maxVal = _window[0];

  for (uint8_t i = 1; i < WINDOW_SIZE; i++) {
    if (_window[i] < minVal) {
      minVal = _window[i];
    }
    if (_window[i] > maxVal) {
      maxVal = _window[i];
    }
  }

  // Check if the variation is within the threshold
  if ((maxVal - minVal) <= STABILITY_THRESHOLD) {
    _isStable = true;
  } else {
    _isStable = false;
  }

  return _isStable;
}

bool StabilityDetector::isStable() const { return _isStable; }

} // namespace Services
