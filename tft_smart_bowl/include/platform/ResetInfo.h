#ifndef PLATFORM_RESET_INFO_H
#define PLATFORM_RESET_INFO_H

#include <stdint.h>

namespace Platform {

/**
 * @brief Human-readable reset reason categories.
 */
enum class ResetReason : uint8_t {
    Unknown = 0,
    PowerOn,
    ExternalReset,
    SoftwareReset,
    DeepSleepWakeup,
    Brownout,
    Watchdog,       // Task WDT or interrupt WDT
    Panic,          // Software panic / exception
    Other
};

/**
 * @brief Captures and reports the reason for the last system reset.
 * 
 * Uses the ESP32 ROM reset reason API to classify what caused
 * the most recent boot. Also tracks consecutive boot failures
 * via NVS to enable safe-mode entry.
 */
class ResetInfo {
public:
    /**
     * @brief Read the reset reason from the ESP32 hardware.
     * Must be called early in setup(), before anything clears the register.
     */
    static void capture();

    /** @brief Get the classified reset reason. */
    static ResetReason reason();

    /** @brief Get a human-readable string for the reset reason. */
    static const char* reasonString();

    /** @brief Get the raw ESP32 reset reason code. */
    static uint32_t rawCode();

    /**
     * @brief Increment the consecutive-boot-failure counter in NVS.
     * Call this early in setup(). If boot completes successfully,
     * call markBootSuccess() to reset the counter.
     * @return The current failure count AFTER incrementing.
     */
    static uint32_t incrementBootFailCount();

    /**
     * @brief Reset the boot-failure counter to 0.
     * Call this after the system has fully initialized successfully.
     */
    static void markBootSuccess();

    /**
     * @brief Get the current consecutive boot failure count without modifying it.
     */
    static uint32_t bootFailCount();

    /** @brief Threshold for entering safe mode. */
    static constexpr uint32_t SAFE_MODE_THRESHOLD = 3;

    /** @brief Returns true if boot failures have reached the safe-mode threshold. */
    static bool shouldEnterSafeMode();

private:
    static ResetReason _reason;
    static uint32_t    _rawCode;
    static uint32_t    _bootFailCount;
    static bool        _captured;
};

} // namespace Platform

#endif // PLATFORM_RESET_INFO_H
