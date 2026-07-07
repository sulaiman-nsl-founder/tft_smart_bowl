#include "services/Calibration.h"
#include "services/Logger.h"
#include <Arduino.h>
#include <Preferences.h>

namespace Services {

static const char* PREF_NAMESPACE = "smartbowl";
static const char* PREF_KEY_CALIB = "calib_v1";

Calibration::Calibration() {
    resetToDefaults();
}

void Calibration::resetToDefaults() {
    _data.magic = MAGIC;
    _data.rawZero = 0;
    _data.calibrationFactor = 1.0f;
    _data.crc = calculateCrc(_data);
    _valid = false;
}

void Calibration::setZeroOffset(int32_t rawZero) {
    _data.rawZero = rawZero;
    _data.crc = calculateCrc(_data);
    _valid = true; 
    LOG_INFO("CAL", 500, "Zero offset set to %d", rawZero);
}

void Calibration::setSpan(int32_t rawSpan, float refGrams) {
    if (rawSpan == _data.rawZero) {
        LOG_ERROR("CAL", 501, "Span rejected: span == zero");
        return;
    }
    
    // factor = grams / raw_diff
    _data.calibrationFactor = refGrams / (float)(rawSpan - _data.rawZero);
    _data.crc = calculateCrc(_data);
    _valid = true;
    LOG_INFO("CAL", 502, "Span set: %d raw -> %.1f g (factor: %f)", 
             rawSpan, refGrams, _data.calibrationFactor);
}

float Calibration::toGrams(int32_t rawReading) const {
    if (!_valid) return 0.0f;
    return (float)(rawReading - _data.rawZero) * _data.calibrationFactor;
}

bool Calibration::load() {
    Preferences prefs;
    prefs.begin(PREF_NAMESPACE, true); // read-only
    
    CalData loadedData;
    size_t len = prefs.getBytes(PREF_KEY_CALIB, &loadedData, sizeof(CalData));
    prefs.end();

    if (len != sizeof(CalData)) {
        LOG_WARN("CAL", 503, "No valid calibration found in NVS");
        resetToDefaults();
        return false;
    }

    if (loadedData.magic != MAGIC) {
        LOG_WARN("CAL", 504, "Calibration magic mismatch");
        resetToDefaults();
        return false;
    }

    uint32_t expectedCrc = calculateCrc(loadedData);
    if (loadedData.crc != expectedCrc) {
        LOG_ERROR("CAL", 505, "Calibration CRC mismatch! Data corrupted.");
        resetToDefaults();
        return false;
    }

    _data = loadedData;
    _valid = true;
    LOG_INFO("CAL", 506, "Loaded calibration: zero=%d, factor=%f", 
             _data.rawZero, _data.calibrationFactor);
    return true;
}

bool Calibration::save() {
    if (!_valid) {
        LOG_WARN("CAL", 507, "Refusing to save invalid calibration");
        return false;
    }

    _data.crc = calculateCrc(_data);

    Preferences prefs;
    prefs.begin(PREF_NAMESPACE, false); // read-write
    size_t len = prefs.putBytes(PREF_KEY_CALIB, &_data, sizeof(CalData));
    prefs.end();

    if (len == sizeof(CalData)) {
        LOG_INFO("CAL", 508, "Calibration saved to NVS");
        return true;
    } else {
        LOG_ERROR("CAL", 509, "Failed to save calibration to NVS");
        return false;
    }
}

bool Calibration::isValid() const {
    return _valid;
}

uint32_t Calibration::calculateCrc(const CalData& data) const {
    // Simple checksum for the struct (excluding the crc field itself)
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&data);
    size_t len = sizeof(CalData) - sizeof(uint32_t);
    
    uint32_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        crc += bytes[i];
    }
    return crc;
}

} // namespace Services
