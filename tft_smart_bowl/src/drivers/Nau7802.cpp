#include "drivers/Nau7802.h"
#include "drivers/I2cBus.h"
#include "services/Logger.h"
#include "core/Errors.h"
#include <Arduino.h>
#include <Wire.h>
#include "SparkFun_Qwiic_Scale_NAU7802_Arduino_Library.h"

namespace Drivers {

// Static instance of the underlying library driver
static NAU7802 s_nau;

Nau7802& Nau7802::getInstance() {
    static Nau7802 instance;
    return instance;
}

Core::Result<void> Nau7802::begin() {
    if (_initialized) {
        return Core::Result<void>();
    }

    auto& i2c = I2cBus::getInstance();

    // NAU7802 begin() does a lot of I2C transactions (reset, power up, LDO config, AFE calibration)
    // We must lock the bus for the entire duration so other threads don't interrupt it.
    if (!i2c.lock(2000)) { // Give it up to 2 seconds to acquire lock, as begin() can take ~300ms
        LOG_ERROR("NAU", 400, "Failed to acquire I2C mutex for begin()");
        return Core::Result<void>(Core::Error(
            Core::ErrorCategory::I2C, Core::ErrorCode::Timeout, "Mutex timeout"));
    }

    // Pass the standard Wire instance; our I2cBus has already initialized it
    bool success = s_nau.begin(Wire, true); // true = reset sensor
    
    i2c.unlock();

    if (!success) {
        LOG_ERROR("NAU", 401, "NAU7802 initialization failed");
        return Core::Result<void>(Core::Error(
            Core::ErrorCategory::I2C, Core::ErrorCode::DeviceNotFound, "NAU7802 not detected"));
    }

    // Optionally set specific gain/rate here, though default is Gain 128, LDO 3.3V, 10SPS
    
    _initialized = true;
    LOG_INFO("NAU", 402, "NAU7802 initialized successfully");
    
    return Core::Result<void>();
}

bool Nau7802::available() {
    if (!_initialized) return false;

    auto& i2c = I2cBus::getInstance();
    bool isAvail = false;

    if (i2c.lock(100)) {
        isAvail = s_nau.available();
        i2c.unlock();
    }
    
    return isAvail;
}

Core::Result<int32_t> Nau7802::getReading() {
    if (!_initialized) {
        return Core::Result<int32_t>(Core::Error(
            Core::ErrorCategory::System, Core::ErrorCode::InvalidState, "NAU7802 not initialized"));
    }

    auto& i2c = I2cBus::getInstance();
    
    if (!i2c.lock(200)) {
        LOG_ERROR("NAU", 403, "Failed to acquire I2C mutex for reading");
        return Core::Result<int32_t>(Core::Error(
            Core::ErrorCategory::I2C, Core::ErrorCode::Timeout, "Mutex timeout"));
    }

    int32_t reading = s_nau.getReading();
    i2c.unlock();

    return Core::Result<int32_t>(reading);
}

bool Nau7802::setSampleRate(uint8_t rate) {
    if (!_initialized) return false;
    
    auto& i2c = I2cBus::getInstance();
    bool success = false;
    
    if (i2c.lock(100)) {
        success = s_nau.setSampleRate(rate);
        i2c.unlock();
    }
    
    if (success) {
        LOG_INFO("NAU", 404, "NAU7802 sample rate updated to %u", rate);
    } else {
        LOG_ERROR("NAU", 405, "Failed to update sample rate");
    }
    
    return success;
}

} // namespace Drivers
