/*
 * NAU7802 Load Cell Calibration & Runtime Weight Reading
 * -------------------------------------------------------
 * Board-agnostic Arduino framework implementation.
 * Uses the SparkFun NAU7802 Arduino Library for AFE calibration,
 * built-in tare/scale-factor calculation, and weight reads.
 *
 * Serial commands (115200 baud, line ending = Newline):
 *   t  -> tare (zero) the scale
 *   c  -> calibrate against a known mass (prompts for grams)
 *   s  -> save current zero offset + calibration factor to persistent storage
 *   r  -> stream filtered weight continuously
 *   d  -> stream RAW uncalibrated ADC reading (use this to verify wiring)
 *
 * Persistent storage: ESP32 uses Preferences (NVS). For other targets
 * (e.g. XIAO nRF54L15), swap out saveCalibration()/loadCalibration()
 * with your platform's flash/NVS API - the values you need to persist
 * are just two numbers: zeroOffset (int32_t) and calFactor (float).
 */

#include "SparkFun_Qwiic_Scale_NAU7802_Arduino_Library.h"
#include <Wire.h>

#if defined(ESP32)
#include <Preferences.h>
Preferences prefs;
#endif

NAU7802 scale;

// ---- Filtering config ----
const uint8_t RAW_AVG_SAMPLES = 8;  // internal averaging per read
const uint8_t MEDIAN_WINDOW = 5;    // outlier rejection window
const float EMA_ALPHA = 0.2f;       // smoothing factor
const float STABLE_DELTA_G = 0.5f;  // grams; "stable" threshold
const uint8_t STABLE_COUNT_REQ = 5; // consecutive stable reads

float emaWeight = 0.0f;
bool emaInit = false;

float medianBuf[MEDIAN_WINDOW];
uint8_t medianIdx = 0;
bool medianFilled = false;

float lastStableCandidate = 0.0f;
uint8_t stableCount = 0;

// Forward declarations
float medianFilter(float newVal);
float emaFilter(float newVal);
void resetFilters();
void checkStable(float weight);
void handleSerialCommands();
void flushSerialInput();
void saveCalibration();
void loadCalibration();

// ---------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }
  Serial.setTimeout(15000); // give yourself 15s to type a value when prompted

  Wire.begin(42, 41);
  Wire.setClock(400000); // fast mode I2C; drop to 100k if you see glitches

  if (!scale.begin()) {
    Serial.println("NAU7802 not detected. Check wiring.");
    while (1) {
      delay(1000);
    }
  }

  scale.setLDO(NAU7802_LDO_3V3);       // match your load cell excitation
  scale.setGain(NAU7802_GAIN_128);     // typical for mV/V strain gauges
  scale.setSampleRate(NAU7802_SPS_20); // lower SPS = cleaner noise floor

  scale.calibrateAFE(); // internal offset/gain cal - required after
                        // changing gain/rate

  loadCalibration();

  Serial.println(
      "Ready. Commands: t=tare  c=calibrate  s=save  r=stream  d=raw dump");
}

// ---------------------------------------------------------------
void loop() {
  handleSerialCommands();

  if (scale.available()) {
    float raw =
        scale.getWeight(true, RAW_AVG_SAMPLES); // allowNegative, samples

    if (isnan(raw) || isinf(raw)) {
      Serial.println("ERROR: invalid weight reading — recalibrate (t then c).");
    } else {
      float med = medianFilter(raw);
      float smooth = emaFilter(med);
      checkStable(smooth);
    }
  }
}

// ---------------------------------------------------------------
void handleSerialCommands() {
  if (!Serial.available())
    return;
  char c = Serial.read();
  flushSerialInput(); // clear the leftover newline from the keypress

  switch (c) {
  case 't':
  case 'T':
    Serial.println("Taring... keep scale empty.");
    scale.calculateZeroOffset(64, 5000);
    Serial.print("Zero offset: ");
    Serial.println(scale.getZeroOffset());
    resetFilters();
    break;

  case 'c':
  case 'C': {
    Serial.println("Place known mass, then enter grams and press Enter:");
    while (!Serial.available()) {
      delay(10);
    } // waits for you to type
    String input = Serial.readStringUntil('\n');
    float knownGrams = input.toFloat();

    if (knownGrams <= 0.0f) {
      Serial.println("Invalid mass entered - calibration aborted.");
      break;
    }

    float factorBefore = scale.getCalibrationFactor(); // fallback if this fails
    scale.calculateCalibrationFactor(knownGrams, 64, 5000);
    float newFactor = scale.getCalibrationFactor();

    if (isnan(newFactor) || isinf(newFactor) || newFactor == 0.0f) {
      Serial.println("Calibration failed — raw reading didn't change.");
      Serial.println("Check wiring, or that the mass was on the scale before "
                     "pressing Enter.");
      Serial.println("Run 'd' to view raw ADC counts and verify the sensor "
                     "responds to load.");
      scale.setCalibrationFactor(
          factorBefore); // roll back, don't leave it broken
      break;
    }

    Serial.print("Calibration factor: ");
    Serial.println(newFactor, 6);
    resetFilters();
    break;
  }

  case 's':
  case 'S':
    saveCalibration();
    Serial.println("Calibration saved.");
    break;

  case 'r':
  case 'R':
    Serial.println("Streaming weight (grams)... send any key to stop.");
    while (!Serial.available()) {
      if (scale.available()) {
        float raw = scale.getWeight(true, RAW_AVG_SAMPLES);
        if (isnan(raw) || isinf(raw)) {
          Serial.println(
              "ERROR: invalid weight reading — recalibrate (t then c).");
        } else {
          float med = medianFilter(raw);
          float smooth = emaFilter(med);
          Serial.println(smooth, 2);
        }
      }
      delay(50);
    }
    flushSerialInput(); // clear the keypress that stopped streaming
    break;

  case 'd':
  case 'D':
    Serial.println(
        "Raw ADC dump — press/release the load cell. Any key to stop.");
    while (!Serial.available()) {
      if (scale.available()) {
        Serial.println(scale.getReading()); // raw, uncalibrated
      }
      delay(100);
    }
    flushSerialInput();
    break;
  }
}

// ---------------------------------------------------------------
void flushSerialInput() {
  while (Serial.available())
    Serial.read();
}

// ---------------------------------------------------------------
// Median-of-N outlier rejection
float medianFilter(float newVal) {
  medianBuf[medianIdx] = newVal;
  medianIdx = (medianIdx + 1) % MEDIAN_WINDOW;
  if (medianIdx == 0)
    medianFilled = true;

  uint8_t n = medianFilled ? MEDIAN_WINDOW : medianIdx;
  if (n == 0)
    return newVal;

  float sorted[MEDIAN_WINDOW];
  memcpy(sorted, medianBuf, n * sizeof(float));
  // simple insertion sort, n is tiny
  for (uint8_t i = 1; i < n; i++) {
    float key = sorted[i];
    int8_t j = i - 1;
    while (j >= 0 && sorted[j] > key) {
      sorted[j + 1] = sorted[j];
      j--;
    }
    sorted[j + 1] = key;
  }
  return sorted[n / 2];
}

// Exponential moving average
float emaFilter(float newVal) {
  if (!emaInit) {
    emaWeight = newVal;
    emaInit = true;
  } else {
    emaWeight = emaWeight + EMA_ALPHA * (newVal - emaWeight);
  }
  return emaWeight;
}

void resetFilters() {
  emaInit = false;
  medianIdx = 0;
  medianFilled = false;
  stableCount = 0;
}

// ---------------------------------------------------------------
// Detects when N consecutive reads are within STABLE_DELTA_G of
// each other, then reports a "stable" reading once.
void checkStable(float weight) {
  if (fabs(weight - lastStableCandidate) <= STABLE_DELTA_G) {
    stableCount++;
  } else {
    stableCount = 0;
  }
  lastStableCandidate = weight;

  if (stableCount == STABLE_COUNT_REQ) {
    Serial.print("STABLE: ");
    Serial.print(weight, 2);
    Serial.println(" g");
  }
}

// ---------------------------------------------------------------
#if defined(ESP32)
void saveCalibration() {
  prefs.begin("loadcell", false);
  prefs.putInt("zeroOffset", scale.getZeroOffset());
  prefs.putFloat("calFactor", scale.getCalibrationFactor());
  prefs.end();
}

void loadCalibration() {
  prefs.begin("loadcell", true);
  int32_t zeroOffset = prefs.getInt("zeroOffset", 0);
  float calFactor = prefs.getFloat("calFactor", 1.0f);
  prefs.end();

  if (calFactor != 1.0f) { // crude "was it ever calibrated" check
    scale.setZeroOffset(zeroOffset);
    scale.setCalibrationFactor(calFactor);
    Serial.println("Loaded saved calibration.");
  } else {
    Serial.println("No saved calibration found - run 't' then 'c'.");
  }
}
#else
// TODO: port to your platform's NVS/flash API (e.g. nRF54L15 NVS).
// You only need to persist two values: scale.getZeroOffset() (int32_t)
// and scale.getCalibrationFactor() (float).
void saveCalibration() {
  Serial.println("saveCalibration() not implemented for this platform.");
}
void loadCalibration() {
  Serial.println("loadCalibration() not implemented for this platform - run "
                 "'t' then 'c' each boot.");
}
#endif