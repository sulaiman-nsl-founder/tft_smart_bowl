#include "drivers/SpiBus.h"
#include "services/Logger.h"
#include <Arduino.h>
#include <SPI.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// Use FSPI as in reference code: SPIClass TFTSPI(FSPI)
static SPIClass s_fspi(FSPI);

namespace Drivers {

SpiBus& SpiBus::getInstance() {
    static SpiBus instance;
    return instance;
}

Core::Result<void> SpiBus::begin(uint8_t mosiPin, uint8_t sclkPin, uint8_t misoPin) {
    if (_initialized) {
        return Core::Result<void>();
    }

    _mutex = xSemaphoreCreateMutex();
    if (_mutex == nullptr) {
        return Core::Result<void>(Core::Error(
            Core::ErrorCategory::System, Core::ErrorCode::NoMemory, "Failed to create SPI mutex"));
    }

    // Initialize FSPI with the specified pins
    int8_t miso = (misoPin == 0xFF) ? -1 : static_cast<int8_t>(misoPin);
    s_fspi.begin(sclkPin, miso, mosiPin);

    _spi = &s_fspi;
    _initialized = true;
    LOG_INFO("SPI", 700, "SPI bus initialized (MOSI=%u, SCLK=%u, MISO=%d)", 
             mosiPin, sclkPin, miso);
    return Core::Result<void>();
}

bool SpiBus::acquire(uint32_t timeoutMs) {
    if (!_initialized || _mutex == nullptr) return false;
    return xSemaphoreTake(static_cast<SemaphoreHandle_t>(_mutex), pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
}

void SpiBus::release() {
    if (!_initialized || _mutex == nullptr) return;
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(_mutex));
}

bool SpiBus::isInitialized() const {
    return _initialized;
}

void* SpiBus::spiInstance() {
    return _spi;
}

} // namespace Drivers
