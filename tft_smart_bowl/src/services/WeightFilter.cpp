#include "services/WeightFilter.h"
#include <stdlib.h> // for abs()

namespace Services {

WeightFilter::WeightFilter() : _filteredValue(0), _isInitialized(false) {}

void WeightFilter::reset(int32_t initialValue) {
  _filteredValue = initialValue;
  _isInitialized = true;
}

int32_t WeightFilter::addSample(int32_t rawSample) {
  if (!_isInitialized) {
    reset(rawSample);
    return _filteredValue;
  }

  int32_t diff = rawSample - _filteredValue;

  // Outlier rejection / heavy damping for large jumps
  if (abs(diff) > OUTLIER_THRESHOLD) {
    // If it's a massive jump, it could be a real weight placed on the scale,
    // or it could be a glitch. We respond very slowly to it initially to let
    // the stability detector catch the transient.
    _filteredValue += (diff * (EWMA_ALPHA_PCT / 4)) / 100;
  } else {
    // Normal EWMA filter
    _filteredValue += (diff * EWMA_ALPHA_PCT) / 100;
  }

  return _filteredValue;
}

int32_t WeightFilter::getFilteredValue() const { return _filteredValue; }

} // namespace Services
