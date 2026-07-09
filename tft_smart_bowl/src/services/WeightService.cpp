#include "services/WeightService.h"
#include "services/EventBus.h"
#include "services/Logger.h"
#include "services/Supervisor.h"
#include <Arduino.h>

namespace Services {

WeightService &WeightService::getInstance() {
  static WeightService instance;
  return instance;
}

void WeightService::begin() {
  if (_taskHandle != nullptr)
    return;

  // Load calibration from NVS
  _calibration.load();

  // The NAU7802 must have been initialized by Application::setup /
  // BoardSelfTest before this task starts, but we ensure the driver is ready.
  if (_nau.begin().isError()) {
    LOG_ERROR("WGT", 600, "NAU7802 begin failed in WeightService");
  }

  xTaskCreatePinnedToCore(taskMain, "WeightTask", 4096, this,
                          5, // Priority (above normal)
                          &_taskHandle,
                          1 // App core
  );
}

void WeightService::taskMain(void *pvParameters) {
  WeightService *service = static_cast<WeightService *>(pvParameters);

  // Register this task with the Supervisor (Watchdog)
  Supervisor::getInstance().registerCurrentTask();

  service->run();
}

void WeightService::run() {
  LOG_INFO("WGT", 601, "WeightService task started");

  while (true) {
    // Feed the watchdog
    Supervisor::getInstance().feed();

    if (_nau.available()) {
      auto result = _nau.getReading();
      if (result.isOk()) {
        int32_t raw = result.value();

        // 1. Filter the raw data
        _currentRaw = _filter.addSample(raw);

        // 2. Check stability
        bool stable = _stability.addSample(_currentRaw);

        // 3. Compute grams
        if (_calibration.isValid()) {
          _currentWeightGrams = _calibration.toGrams(_currentRaw);
        } else {
          _currentWeightGrams = 0.0f; // Force 0 if uncalibrated
        }

        // 4. Handle Calibration State Machine
        if (_calibState != CalibState::Idle) {
          if (stable) {
            if (_calibState == CalibState::WaitingTareStability) {
              _calibration.setZeroOffset(_currentRaw);
              _calibration.save();
              LOG_INFO("WGT", 602, "Tare complete and saved!");
            } else if (_calibState == CalibState::WaitingSpanStability) {
              _calibration.setSpan(_currentRaw, _spanRefGrams);
              _calibration.save();
              LOG_INFO("WGT", 603, "Span complete and saved!");
            }
            _calibState = CalibState::Idle;
          } else {
            // Check for timeout (e.g. 10 seconds = 100 loops at 10Hz)
            if (millis() > _calibTimeout) {
              LOG_WARN("WGT", 604,
                       "Calibration timeout — scale did not stabilize");
              _calibState = CalibState::Idle;
            }
          }
        }

        // 5. Publish weight event
        SystemEvent evt;
        evt.id = EventId::WeightUpdated;
        evt.timestampMs = millis();
        evt.payload.weight.grams = _currentWeightGrams;
        evt.payload.weight.isStable = stable;
        EventBus::getInstance().publish(evt);
      }
    }

    // Wait 100ms (10Hz target rate, NAU7802 default is 10 SPS anyway)
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

float WeightService::getWeight() const { return _currentWeightGrams; }

bool WeightService::isStable() const { return _stability.isStable(); }

bool WeightService::isCalibrated() const { return _calibration.isValid(); }

int32_t WeightService::getRawFiltered() const { return _currentRaw; }

WeightService::CalibState WeightService::getCalibState() const { return _calibState; }

void WeightService::requestTare() {
  if (_calibState != CalibState::Idle)
    return;
  LOG_INFO("WGT", 605, "Tare requested. Waiting for stability...");
  _calibState = CalibState::WaitingTareStability;
  _calibTimeout = millis() + 10000; // 10 second timeout
}

void WeightService::requestSpan(float referenceGrams) {
  if (_calibState != CalibState::Idle)
    return;
  LOG_INFO("WGT", 606, "Span requested for %.1fg. Waiting for stability...",
           referenceGrams);
  _spanRefGrams = referenceGrams;
  _calibState = CalibState::WaitingSpanStability;
  _calibTimeout = millis() + 10000; // 10 second timeout
}

} // namespace Services
