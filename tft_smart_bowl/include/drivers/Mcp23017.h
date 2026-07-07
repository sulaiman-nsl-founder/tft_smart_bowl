#ifndef DRIVERS_MCP23017_H
#define DRIVERS_MCP23017_H

#include <stdint.h>
#include "core/Result.h"
#include "core/Errors.h"

namespace Drivers {

/**
 * @brief MCP23017 I/O expander driver.
 *
 * Based on reference code MapButtonsToLEDs_MCP23017.ino.
 * Uses the Adafruit MCP23X17 library internally.
 * Provides probe, direction, pull-up, digital I/O,
 * and fault state tracking.
 */
class Mcp23017 {
public:
    static Mcp23017& getInstance();

    /**
     * @brief Initialize the MCP23017 at the given I2C address.
     * @param address 7-bit I2C address (default 0x20).
     * @return Success or error if device not found.
     */
    Core::Result<void> begin(uint8_t address = 0x20);

    /**
     * @brief Check if the device is present and responding.
     */
    bool probe();

    /**
     * @brief Configure a pin as INPUT, OUTPUT, or INPUT_PULLUP.
     * @param pin MCP pin number (0-15, Port A = 0-7, Port B = 8-15).
     * @param mode Arduino pin mode (INPUT, OUTPUT, INPUT_PULLUP).
     */
    void setPinMode(uint8_t pin, uint8_t mode);

    /**
     * @brief Read a digital pin.
     * @param pin MCP pin number (0-15).
     * @return Pin state (HIGH/LOW).
     */
    bool digitalRead(uint8_t pin);

    /**
     * @brief Write a digital pin.
     * @param pin MCP pin number (0-15).
     * @param value HIGH or LOW.
     */
    void digitalWrite(uint8_t pin, bool value);

    /**
     * @brief Read all 8 pins of Port A as a byte.
     */
    uint8_t readPortA();

    /**
     * @brief Read all 8 pins of Port B as a byte.
     */
    uint8_t readPortB();

    /**
     * @brief Write all 8 pins of Port B as a byte.
     */
    void writePortB(uint8_t value);

    /** @brief Check if the driver is initialized. */
    bool isInitialized() const;

    /** @brief Check if in fault state (I2C errors detected). */
    bool isFaulted() const;

    /** @brief Get I2C error count for this device. */
    uint32_t errorCount() const;

private:
    Mcp23017() = default;
    ~Mcp23017() = default;
    Mcp23017(const Mcp23017&) = delete;
    Mcp23017& operator=(const Mcp23017&) = delete;

    bool _initialized = false;
    bool _faulted = false;
    uint8_t _address = 0x20;
    uint32_t _errorCount = 0;
    void* _mcpDevice = nullptr;  // Pointer to Adafruit_MCP23X17
};

} // namespace Drivers

#endif // DRIVERS_MCP23017_H
