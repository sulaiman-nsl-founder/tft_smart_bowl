#include "drivers/Mcp23017.h"
#include "services/Logger.h"
#include <Adafruit_MCP23X17.h>
#include <Wire.h>

namespace Drivers {

// Static instance of the Adafruit driver (avoid dynamic allocation)
static Adafruit_MCP23X17 s_mcp;

Mcp23017& Mcp23017::getInstance() {
    static Mcp23017 instance;
    return instance;
}

Core::Result<void> Mcp23017::begin(uint8_t address) {
    _address = address;

    // Reference: MapButtonsToLEDs_MCP23017.ino uses mcp.begin_I2C(0x20, &Wire)
    if (!s_mcp.begin_I2C(address, &Wire)) {
        _faulted = true;
        _errorCount++;
        LOG_ERROR("MCP", 400, "MCP23017 not found at 0x%02X", address);
        return Core::Result<void>(Core::Error(
            Core::ErrorCategory::I2C, Core::ErrorCode::DeviceNotFound,
            "MCP23017 not detected"));
    }

    _mcpDevice = &s_mcp;
    _initialized = true;
    _faulted = false;
    LOG_INFO("MCP", 401, "MCP23017 connected at 0x%02X", address);
    return Core::Result<void>();
}

bool Mcp23017::probe() {
    // Try a quick begin to see if device responds
    Wire.beginTransmission(_address);
    uint8_t result = Wire.endTransmission();
    return (result == 0);
}

void Mcp23017::setPinMode(uint8_t pin, uint8_t mode) {
    if (!_initialized) return;
    s_mcp.pinMode(pin, mode);
}

bool Mcp23017::digitalRead(uint8_t pin) {
    if (!_initialized) return false;
    return s_mcp.digitalRead(pin);
}

void Mcp23017::digitalWrite(uint8_t pin, bool value) {
    if (!_initialized) return;
    s_mcp.digitalWrite(pin, value ? HIGH : LOW);
}

uint8_t Mcp23017::readPortA() {
    if (!_initialized) return 0;
    return s_mcp.readGPIO(0);  // Port A = 0
}

uint8_t Mcp23017::readPortB() {
    if (!_initialized) return 0;
    return s_mcp.readGPIO(1);  // Port B = 1
}

void Mcp23017::writePortB(uint8_t value) {
    if (!_initialized) return;
    s_mcp.writeGPIO(value, 1);  // Port B = 1
}

bool Mcp23017::isInitialized() const {
    return _initialized;
}

bool Mcp23017::isFaulted() const {
    return _faulted;
}

uint32_t Mcp23017::errorCount() const {
    return _errorCount;
}

} // namespace Drivers
