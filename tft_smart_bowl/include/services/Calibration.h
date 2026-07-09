#ifndef SERVICES_CALIBRATION_H
#define SERVICES_CALIBRATION_H

#include "core/Result.h"
#include <stdint.h>

namespace Services {

/**
 * @brief Represents the calibration parameters for the load cell.
 *
 * Capable of saving/loading itself to/from NVS, validating data integrity.
 */
class Calibration {
public:
  static constexpr uint32_t MAGIC = 0xC41B0001; // "CALB" + version 1

  Calibration();

  /**
   * @brief Reset to default uncalibrated state.
   */
  void resetToDefaults();

  /**
   * @brief Set the zero offset (raw ADC value when empty).
   */
  void setZeroOffset(int32_t rawZero);

  /**
   * @brief Set the span calibration.
   * @param rawSpan Raw ADC value with the reference weight.
   * @param refGrams The reference weight in grams.
   */
  void setSpan(int32_t rawSpan, float refGrams);

  /**
   * @brief Convert a raw ADC reading to grams using current calibration.
   * @param rawReading Filtered ADC reading.
   * @return Weight in grams.
   */
  float toGrams(int32_t rawReading) const;

  /**
   * @brief Load calibration from NVS.
   * @return true if valid calibration was loaded.
   */
  bool load();

  /**
   * @brief Save current calibration to NVS.
   * @return true if saved successfully.
   */
  bool save();

  /**
   * @brief Check if a valid calibration has been performed and loaded.
   */
  bool isValid() const;

private:
  struct CalData {
    uint32_t magic;
    int32_t rawZero;
    float calibrationFactor;
    uint32_t crc; // CRC32 of the above fields
  };

  CalData _data;
  bool _valid;

  uint32_t calculateCrc(const CalData &data) const;
};

} // namespace Services

#endif // SERVICES_CALIBRATION_H
