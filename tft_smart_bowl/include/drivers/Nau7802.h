#ifndef DRIVERS_NAU7802_H
#define DRIVERS_NAU7802_H

#include "core/Result.h"
#include <stdint.h>

namespace Drivers {

/**
 * @brief Thread-safe low-level driver for the NAU7802 24-bit ADC.
 *
 * Provides mutex-protected raw access to the load cell amplifier.
 * Note: Calibration, sample filtering, and tare functionality are handled
 * at a higher level (WeightService). This driver only handles hardware
 * configuration and raw ADC readings.
 */
class Nau7802 {
public:
  static Nau7802 &getInstance();

  /**
   * @brief Initialize the NAU7802 over I2C.
   * Starts the device with LDO=3.3V, Gain=128, and performs AFE calibration.
   * @return Success or error result.
   */
  Core::Result<void> begin();

  /**
   * @brief Check if a new ADC conversion is ready.
   * @return true if data is available.
   */
  bool available();

  /**
   * @brief Read the raw 24-bit signed ADC value.
   * @return The raw reading, or an error if read fails.
   */
  Core::Result<int32_t> getReading();

  /**
   * @brief Set the sampling rate.
   * @param rate 0=10SPS, 1=20SPS, 2=40SPS, 3=80SPS, 7=320SPS (matches SparkFun
   * NAU7802_SPS_Values)
   * @return true if successful.
   */
  bool setSampleRate(uint8_t rate);

private:
  Nau7802() = default;
  ~Nau7802() = default;
  Nau7802(const Nau7802 &) = delete;
  Nau7802 &operator=(const Nau7802 &) = delete;

  bool _initialized = false;
};

} // namespace Drivers

#endif // DRIVERS_NAU7802_H
