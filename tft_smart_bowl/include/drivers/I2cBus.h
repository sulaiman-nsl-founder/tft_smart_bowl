#ifndef DRIVERS_I2C_BUS_H
#define DRIVERS_I2C_BUS_H

#include <stdint.h>
#include <stddef.h>
#include "core/Result.h"
#include "core/Errors.h"

namespace Drivers {

/**
 * @brief Thread-safe I2C bus manager with error tracking and recovery.
 *
 * Wraps the Arduino Wire library with:
 * - Mutex-protected transactions (FreeRTOS semaphore)
 * - Bounded transaction timeouts
 * - Bus scan / device probe
 * - Error counters per-device
 * - Stuck-bus (SDA held low) recovery
 */
class I2cBus {
public:
    static I2cBus& getInstance();

    /**
     * @brief Initialize the I2C bus.
     * @param sdaPin SDA GPIO pin number.
     * @param sclPin SCL GPIO pin number.
     * @param freqHz Clock frequency in Hz (default 100kHz).
     * @return Success or error.
     */
    Core::Result<void> begin(uint8_t sdaPin, uint8_t sclPin, uint32_t freqHz = 100000);

    /**
     * @brief Write data to a device.
     * @param address 7-bit I2C address.
     * @param data Pointer to data buffer.
     * @param len Number of bytes to write.
     * @param timeoutMs Maximum time to wait for bus access.
     * @return Success or error.
     */
    Core::Result<void> write(uint8_t address, const uint8_t* data, size_t len, uint32_t timeoutMs = 100);

    /**
     * @brief Read data from a device.
     * @param address 7-bit I2C address.
     * @param buffer Pointer to receive buffer.
     * @param len Number of bytes to read.
     * @param timeoutMs Maximum time to wait for bus access.
     * @return Number of bytes actually read, or error.
     */
    Core::Result<size_t> read(uint8_t address, uint8_t* buffer, size_t len, uint32_t timeoutMs = 100);

    /**
     * @brief Write a register address, then read data (combined transaction).
     * @param address 7-bit I2C address.
     * @param reg Register address to write first.
     * @param buffer Pointer to receive buffer.
     * @param len Number of bytes to read.
     * @param timeoutMs Maximum time to wait for bus access.
     * @return Number of bytes actually read, or error.
     */
    Core::Result<size_t> writeRead(uint8_t address, uint8_t reg, uint8_t* buffer, size_t len, uint32_t timeoutMs = 100);

    /**
     * @brief Check if a device responds at the given address.
     * @param address 7-bit I2C address.
     * @return true if ACK received.
     */
    bool probe(uint8_t address);

    /**
     * @brief Scan the bus and log all responding addresses.
     * @return Number of devices found.
     */
    uint8_t scan();

    /**
     * @brief Attempt to recover a stuck bus (SDA held low).
     * Generates clock pulses to free the bus.
     * @return true if recovery succeeded.
     */
    bool recoverBus();

    /** 
     * @brief Lock the I2C bus mutex manually.
     * Required when passing the underlying Wire object to external libraries (e.g. NAU7802).
     */
    bool lock(uint32_t timeoutMs = 100);

    /** 
     * @brief Unlock the I2C bus mutex manually.
     */
    void unlock();

    /** @brief Get total error count since boot. */
    uint32_t errorCount() const;

    /** @brief Get total successful transaction count since boot. */
    uint32_t successCount() const;

    /** @brief Check if bus has been initialized. */
    bool isInitialized() const;

private:
    I2cBus() = default;
    ~I2cBus() = default;
    I2cBus(const I2cBus&) = delete;
    I2cBus& operator=(const I2cBus&) = delete;

    bool _initialized = false;
    uint8_t _sdaPin = 0;
    uint8_t _sclPin = 0;
    uint32_t _errorCount = 0;
    uint32_t _successCount = 0;
    void* _mutex = nullptr;  // FreeRTOS SemaphoreHandle_t
};

} // namespace Drivers

#endif // DRIVERS_I2C_BUS_H
