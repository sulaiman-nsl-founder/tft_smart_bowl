#ifndef SERVICES_WEIGHT_FILTER_H
#define SERVICES_WEIGHT_FILTER_H

#include <stdint.h>

namespace Services {

/**
 * @brief Filters raw ADC weight samples.
 *
 * Implements a simple, robust filtering strategy:
 * - Maintains a sliding window of recent samples.
 * - Outlier rejection (ignores samples that deviate too far from the current
 * filtered value).
 * - Exponential Weighted Moving Average (EWMA) for smooth tracking.
 */
class WeightFilter {
public:
  WeightFilter();

  /**
   * @brief Reset the filter to a starting value.
   * @param initialValue The raw value to seed the filter with.
   */
  void reset(int32_t initialValue);

  /**
   * @brief Add a new raw sample and compute the new filtered value.
   * @param rawSample The new raw ADC reading.
   * @return The updated filtered value.
   */
  int32_t addSample(int32_t rawSample);

  /**
   * @brief Get the current filtered value.
   * @return The filtered value.
   */
  int32_t getFilteredValue() const;

private:
  int32_t _filteredValue;
  bool _isInitialized;

  // EWMA Alpha: Higher = responds faster, Lower = smoother.
  // Represents alpha as a fraction of 100 to avoid floating point in the fast
  // path. e.g., 20 = 0.20
  static constexpr int32_t EWMA_ALPHA_PCT = 20;

  // Maximum allowed delta for a single sample before it's considered an
  // outlier. If a sample jumps more than this, we heavily dampen it initially.
  // (This value needs tuning based on actual load cell noise profile)
  static constexpr int32_t OUTLIER_THRESHOLD = 50000;
};

} // namespace Services

#endif // SERVICES_WEIGHT_FILTER_H
