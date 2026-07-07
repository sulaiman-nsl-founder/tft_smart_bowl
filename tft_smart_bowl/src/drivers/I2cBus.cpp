#include "drivers/I2cBus.h"
#include "board/Pins.h"
#include "services/Logger.h"
#include <Arduino.h>
#include <Wire.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace Drivers {

I2cBus& I2cBus::getInstance() {
    static I2cBus instance;
    return instance;
}

Core::Result<void> I2cBus::begin(uint8_t sdaPin, uint8_t sclPin, uint32_t freqHz) {
    if (_initialized) {
        return Core::Result<void>(); // Already initialized
    }

    _sdaPin = sdaPin;
    _sclPin = sclPin;

    // Create mutex for thread-safe access
    _mutex = xSemaphoreCreateMutex();
    if (_mutex == nullptr) {
        return Core::Result<void>(Core::Error(
            Core::ErrorCategory::System, Core::ErrorCode::NoMemory, "Failed to create I2C mutex"));
    }

    // Initialize Wire
    if (!Wire.begin(sdaPin, sclPin, freqHz)) {
        LOG_ERROR("I2C", 300, "Wire.begin failed (SDA=%u, SCL=%u)", sdaPin, sclPin);
        return Core::Result<void>(Core::Error(
            Core::ErrorCategory::I2C, Core::ErrorCode::BusInitFailed, "Wire.begin failed"));
    }

    _initialized = true;
    LOG_INFO("I2C", 301, "I2C bus initialized (SDA=%u, SCL=%u, %u Hz)", sdaPin, sclPin, freqHz);
    return Core::Result<void>();
}

Core::Result<void> I2cBus::write(uint8_t address, const uint8_t* data, size_t len, uint32_t timeoutMs) {
    if (!_initialized) {
        return Core::Result<void>(Core::Error(
            Core::ErrorCategory::I2C, Core::ErrorCode::InvalidState, "I2C not initialized"));
    }

    if (xSemaphoreTake(static_cast<SemaphoreHandle_t>(_mutex), pdMS_TO_TICKS(timeoutMs)) != pdTRUE) {
        _errorCount++;
        return Core::Result<void>(Core::Error(
            Core::ErrorCategory::I2C, Core::ErrorCode::Timeout, "I2C mutex timeout"));
    }

    Wire.beginTransmission(address);
    Wire.write(data, len);
    uint8_t result = Wire.endTransmission();

    xSemaphoreGive(static_cast<SemaphoreHandle_t>(_mutex));

    if (result != 0) {
        _errorCount++;
        const char* msg = "Unknown I2C error";
        switch (result) {
            case 1: msg = "Data too long"; break;
            case 2: msg = "NACK on address"; break;
            case 3: msg = "NACK on data"; break;
            case 4: msg = "Other error"; break;
            case 5: msg = "Timeout"; break;
        }
        return Core::Result<void>(Core::Error(
            Core::ErrorCategory::I2C, Core::ErrorCode::DeviceWriteFailed, msg));
    }

    _successCount++;
    return Core::Result<void>();
}

Core::Result<size_t> I2cBus::read(uint8_t address, uint8_t* buffer, size_t len, uint32_t timeoutMs) {
    if (!_initialized) {
        return Core::Result<size_t>(Core::Error(
            Core::ErrorCategory::I2C, Core::ErrorCode::InvalidState, "I2C not initialized"));
    }

    if (xSemaphoreTake(static_cast<SemaphoreHandle_t>(_mutex), pdMS_TO_TICKS(timeoutMs)) != pdTRUE) {
        _errorCount++;
        return Core::Result<size_t>(Core::Error(
            Core::ErrorCategory::I2C, Core::ErrorCode::Timeout, "I2C mutex timeout"));
    }

    size_t received = Wire.requestFrom(address, len);
    for (size_t i = 0; i < received && Wire.available(); i++) {
        buffer[i] = Wire.read();
    }

    xSemaphoreGive(static_cast<SemaphoreHandle_t>(_mutex));

    if (received == 0) {
        _errorCount++;
        return Core::Result<size_t>(Core::Error(
            Core::ErrorCategory::I2C, Core::ErrorCode::DeviceReadFailed, "No bytes received"));
    }

    _successCount++;
    return Core::Result<size_t>(received);
}

Core::Result<size_t> I2cBus::writeRead(uint8_t address, uint8_t reg, uint8_t* buffer, size_t len, uint32_t timeoutMs) {
    if (!_initialized) {
        return Core::Result<size_t>(Core::Error(
            Core::ErrorCategory::I2C, Core::ErrorCode::InvalidState, "I2C not initialized"));
    }

    if (xSemaphoreTake(static_cast<SemaphoreHandle_t>(_mutex), pdMS_TO_TICKS(timeoutMs)) != pdTRUE) {
        _errorCount++;
        return Core::Result<size_t>(Core::Error(
            Core::ErrorCategory::I2C, Core::ErrorCode::Timeout, "I2C mutex timeout"));
    }

    // Write the register address
    Wire.beginTransmission(address);
    Wire.write(reg);
    uint8_t result = Wire.endTransmission(false); // Repeated start

    if (result != 0) {
        xSemaphoreGive(static_cast<SemaphoreHandle_t>(_mutex));
        _errorCount++;
        return Core::Result<size_t>(Core::Error(
            Core::ErrorCategory::I2C, Core::ErrorCode::DeviceWriteFailed, "Register write failed"));
    }

    // Read the data
    size_t received = Wire.requestFrom(address, len);
    for (size_t i = 0; i < received && Wire.available(); i++) {
        buffer[i] = Wire.read();
    }

    xSemaphoreGive(static_cast<SemaphoreHandle_t>(_mutex));

    if (received == 0) {
        _errorCount++;
        return Core::Result<size_t>(Core::Error(
            Core::ErrorCategory::I2C, Core::ErrorCode::DeviceReadFailed, "No bytes received after register write"));
    }

    _successCount++;
    return Core::Result<size_t>(received);
}

bool I2cBus::probe(uint8_t address) {
    if (!_initialized) return false;

    if (xSemaphoreTake(static_cast<SemaphoreHandle_t>(_mutex), pdMS_TO_TICKS(100)) != pdTRUE) {
        return false;
    }

    Wire.beginTransmission(address);
    uint8_t result = Wire.endTransmission();

    xSemaphoreGive(static_cast<SemaphoreHandle_t>(_mutex));
    return (result == 0);
}

uint8_t I2cBus::scan() {
    if (!_initialized) {
        LOG_WARN("I2C", 310, "Cannot scan: I2C not initialized");
        return 0;
    }

    LOG_INFO("I2C", 311, "--- I2C Bus Scan ---");
    uint8_t count = 0;

    for (uint8_t addr = 1; addr < 127; addr++) {
        if (probe(addr)) {
            LOG_INFO("I2C", 312, "Device found at 0x%02X", addr);
            count++;
        }
    }

    LOG_INFO("I2C", 313, "Scan complete: %u device(s) found", count);
    return count;
}

bool I2cBus::recoverBus() {
    LOG_WARN("I2C", 320, "Attempting I2C bus recovery...");

    // Release Wire so we can bit-bang the clock
    Wire.end();

    // Configure SCL as output and SDA as input
    pinMode(_sclPin, OUTPUT);
    pinMode(_sdaPin, INPUT_PULLUP);

    // Generate up to 9 clock pulses to free a stuck slave
    bool recovered = false;
    for (int i = 0; i < 9; i++) {
        digitalWrite(_sclPin, HIGH);
        delayMicroseconds(5);

        // Check if SDA is released
        if (digitalRead(_sdaPin) == HIGH) {
            recovered = true;
            break;
        }

        digitalWrite(_sclPin, LOW);
        delayMicroseconds(5);
    }

    // Generate a STOP condition
    pinMode(_sdaPin, OUTPUT);
    digitalWrite(_sdaPin, LOW);
    delayMicroseconds(5);
    digitalWrite(_sclPin, HIGH);
    delayMicroseconds(5);
    digitalWrite(_sdaPin, HIGH);
    delayMicroseconds(5);

    // Re-initialize Wire
    Wire.begin(_sdaPin, _sclPin);

    if (recovered) {
        LOG_INFO("I2C", 321, "Bus recovery succeeded");
    } else {
        LOG_ERROR("I2C", 322, "Bus recovery FAILED — SDA still held low");
        _errorCount++;
    }

    return recovered;
}

uint32_t I2cBus::errorCount() const {
    return _errorCount;
}

uint32_t I2cBus::successCount() const {
    return _successCount;
}

bool I2cBus::isInitialized() const {
    return _initialized;
}

bool I2cBus::lock(uint32_t timeoutMs) {
    if (!_initialized || _mutex == nullptr) return false;
    return (xSemaphoreTake(static_cast<SemaphoreHandle_t>(_mutex), pdMS_TO_TICKS(timeoutMs)) == pdTRUE);
}

void I2cBus::unlock() {
    if (_initialized && _mutex != nullptr) {
        xSemaphoreGive(static_cast<SemaphoreHandle_t>(_mutex));
    }
}

} // namespace Drivers
