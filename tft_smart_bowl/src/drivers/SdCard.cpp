#include "drivers/SdCard.h"
#include "drivers/SpiBus.h"
#include "board/Pins.h"
#include "services/Logger.h"
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>

namespace Drivers {

SdCard& SdCard::getInstance() {
    static SdCard instance;
    return instance;
}

Core::Result<void> SdCard::begin() {
    if (_isMounted) return Core::Result<void>();

    auto& spiBus = SpiBus::getInstance();
    if (!spiBus.isInitialized()) {
        return Core::Result<void>(Core::Error(
            Core::ErrorCategory::SPI, Core::ErrorCode::InvalidState, "SPI bus not initialized"));
    }

    SPIClass* spi = static_cast<SPIClass*>(spiBus.spiInstance());
    
    // We pass our shared SPI instance to the SD library.
    // The underlying ESP32 core will handle hardware locking for transactions.
    if (!SD.begin(Board::Pins::SD_CS, *spi, 4000000U)) {
        LOG_ERROR("SD", 710, "Failed to mount SD card");
        return Core::Result<void>(Core::Error(
            Core::ErrorCategory::System, Core::ErrorCode::DeviceNotFound, "SD Card mount failed"));
    }

    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        LOG_ERROR("SD", 711, "No SD card attached");
        return Core::Result<void>(Core::Error(
            Core::ErrorCategory::System, Core::ErrorCode::DeviceNotFound, "No SD card attached"));
    }

    _isMounted = true;
    
    LOG_INFO("SD", 712, "SD Card mounted. Type: %s, Size: %llu MB", 
             (cardType == CARD_MMC) ? "MMC" : (cardType == CARD_SD) ? "SDSC" : (cardType == CARD_SDHC) ? "SDHC" : "UNKNOWN",
             getCapacity() / (1024 * 1024));
             
    return Core::Result<void>();
}

bool SdCard::isMounted() const {
    return _isMounted;
}

uint64_t SdCard::getCapacity() const {
    if (!_isMounted) return 0;
    return SD.cardSize();
}

uint64_t SdCard::getFreeSpace() const {
    if (!_isMounted) return 0;
    // SD.totalBytes() / SD.usedBytes() are ESP32 specific extensions to the SD library
    return SD.totalBytes() - SD.usedBytes();
}

} // namespace Drivers
