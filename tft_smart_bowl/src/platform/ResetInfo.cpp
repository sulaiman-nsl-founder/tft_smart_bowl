#include "platform/ResetInfo.h"
#include <Arduino.h>
#include <Preferences.h>

// ESP32-S3 reset reason API
#include "esp_system.h"

// For ESP32-S3 with Arduino core, use esp_reset_reason()
// The rom/rtc_cntl.h header is not available on all ESP32 variants

namespace Platform {

// Static member initialization
ResetReason ResetInfo::_reason       = ResetReason::Unknown;
uint32_t    ResetInfo::_rawCode      = 0;
uint32_t    ResetInfo::_bootFailCount = 0;
bool        ResetInfo::_captured     = false;

void ResetInfo::capture() {
    if (_captured) return;
    _captured = true;

    // Use the portable esp_reset_reason() API
    esp_reset_reason_t raw = esp_reset_reason();
    _rawCode = static_cast<uint32_t>(raw);

    switch (raw) {
        case ESP_RST_POWERON:
            _reason = ResetReason::PowerOn;
            break;
        case ESP_RST_EXT:
            _reason = ResetReason::ExternalReset;
            break;
        case ESP_RST_SW:
            _reason = ResetReason::SoftwareReset;
            break;
        case ESP_RST_PANIC:
            _reason = ResetReason::Panic;
            break;
        case ESP_RST_INT_WDT:
        case ESP_RST_TASK_WDT:
        case ESP_RST_WDT:
            _reason = ResetReason::Watchdog;
            break;
        case ESP_RST_DEEPSLEEP:
            _reason = ResetReason::DeepSleepWakeup;
            break;
        case ESP_RST_BROWNOUT:
            _reason = ResetReason::Brownout;
            break;
        default:
            _reason = ResetReason::Other;
            break;
    }
}

ResetReason ResetInfo::reason() {
    return _reason;
}

const char* ResetInfo::reasonString() {
    switch (_reason) {
        case ResetReason::PowerOn:          return "Power-On";
        case ResetReason::ExternalReset:    return "External Reset";
        case ResetReason::SoftwareReset:    return "Software Reset";
        case ResetReason::DeepSleepWakeup:  return "Deep Sleep Wakeup";
        case ResetReason::Brownout:         return "Brownout";
        case ResetReason::Watchdog:         return "Watchdog";
        case ResetReason::Panic:            return "Panic";
        case ResetReason::Other:            return "Other";
        default:                            return "Unknown";
    }
}

uint32_t ResetInfo::rawCode() {
    return _rawCode;
}

uint32_t ResetInfo::incrementBootFailCount() {
    Preferences prefs;
    prefs.begin("reset_info", false); // read-write
    _bootFailCount = prefs.getUInt("boot_fail", 0);
    _bootFailCount++;
    prefs.putUInt("boot_fail", _bootFailCount);
    prefs.end();
    return _bootFailCount;
}

void ResetInfo::markBootSuccess() {
    Preferences prefs;
    prefs.begin("reset_info", false);
    prefs.putUInt("boot_fail", 0);
    prefs.end();
    _bootFailCount = 0;
}

uint32_t ResetInfo::bootFailCount() {
    if (_bootFailCount == 0) {
        Preferences prefs;
        prefs.begin("reset_info", true); // read-only
        _bootFailCount = prefs.getUInt("boot_fail", 0);
        prefs.end();
    }
    return _bootFailCount;
}

bool ResetInfo::shouldEnterSafeMode() {
    return _bootFailCount >= SAFE_MODE_THRESHOLD;
}

} // namespace Platform
