#ifndef DRIVERS_SPI_BUS_H
#define DRIVERS_SPI_BUS_H

#include <stdint.h>
#include "core/Result.h"
#include "core/Errors.h"

namespace Drivers {

/**
 * @brief SPI bus manager with mutex-protected access.
 *
 * Manages FSPI bus shared between TFT display and SD card.
 * Based on reference code BLEWiFiProvisioner.ino (SPIClass TFTSPI(FSPI)).
 */
class SpiBus {
public:
    static SpiBus& getInstance();

    /**
     * @brief Initialize the SPI bus.
     * @param mosiPin MOSI GPIO.
     * @param sclkPin SCLK GPIO.
     * @param misoPin MISO GPIO (optional, use 0xFF for none).
     * @return Success or error.
     */
    Core::Result<void> begin(uint8_t mosiPin, uint8_t sclkPin, uint8_t misoPin = 0xFF);

    /**
     * @brief Acquire the SPI bus mutex.
     * @param timeoutMs Maximum wait time.
     * @return true if acquired.
     */
    bool acquire(uint32_t timeoutMs = 100);

    /**
     * @brief Release the SPI bus mutex.
     */
    void release();

    /** @brief Check if bus has been initialized. */
    bool isInitialized() const;

    /** @brief Get a pointer to the underlying SPIClass. */
    void* spiInstance();

private:
    SpiBus() = default;
    ~SpiBus() = default;
    SpiBus(const SpiBus&) = delete;
    SpiBus& operator=(const SpiBus&) = delete;

    bool _initialized = false;
    void* _mutex = nullptr;
    void* _spi = nullptr;    // SPIClass pointer
};

} // namespace Drivers

#endif // DRIVERS_SPI_BUS_H
