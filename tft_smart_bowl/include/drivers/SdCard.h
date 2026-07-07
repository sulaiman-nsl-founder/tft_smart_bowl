#ifndef DRIVERS_SD_CARD_H
#define DRIVERS_SD_CARD_H

#include <stdint.h>
#include "core/Result.h"

namespace Drivers {

/**
 * @brief Manages the microSD card.
 */
class SdCard {
public:
    static SdCard& getInstance();

    /**
     * @brief Initialize and mount the SD card.
     * @return Success if mounted, error otherwise.
     */
    Core::Result<void> begin();

    /**
     * @brief Check if SD card is currently mounted and accessible.
     */
    bool isMounted() const;

    /**
     * @brief Get total capacity of the SD card in bytes.
     */
    uint64_t getCapacity() const;

    /**
     * @brief Get free space on the SD card in bytes.
     */
    uint64_t getFreeSpace() const;

private:
    SdCard() = default;
    ~SdCard() = default;
    SdCard(const SdCard&) = delete;
    SdCard& operator=(const SdCard&) = delete;

    bool _isMounted = false;
};

} // namespace Drivers

#endif // DRIVERS_SD_CARD_H
