#ifndef SERVICES_WEIGHT_SERVICE_H
#define SERVICES_WEIGHT_SERVICE_H

#include "drivers/Nau7802.h"
#include "services/WeightFilter.h"
#include "services/StabilityDetector.h"
#include "services/Calibration.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace Services {

/**
 * @brief High-level service managing the load cell.
 * 
 * Runs a dedicated FreeRTOS task at 10Hz to:
 * - Poll the NAU7802
 * - Filter the raw data
 * - Detect stability
 * - Apply calibration
 * - Handle Tare and Span calibration requests
 */
class WeightService {
public:
    enum class CalibState {
        Idle,
        WaitingTareStability,
        WaitingSpanStability
    };

    static WeightService& getInstance();

    /**
     * @brief Initialize the service and start the FreeRTOS task.
     */
    void begin();

    /**
     * @brief Get the latest calculated weight in grams.
     */
    float getWeight() const;

    /**
     * @brief Check if the scale is currently stable.
     */
    bool isStable() const;

    /**
     * @brief Check if the scale has a valid calibration loaded.
     */
    bool isCalibrated() const;

    /**
     * @brief Request a Tare operation (zero the scale).
     * The service will wait for stability before applying the tare.
     */
    void requestTare();

    /**
     * @brief Request a Span operation (calibrate with known weight).
     * @param referenceGrams The known weight placed on the scale.
     */
    void requestSpan(float referenceGrams);

private:
    WeightService() = default;
    ~WeightService() = default;
    WeightService(const WeightService&) = delete;
    WeightService& operator=(const WeightService&) = delete;

    static void taskMain(void* pvParameters);
    void run();

    Drivers::Nau7802& _nau = Drivers::Nau7802::getInstance();
    WeightFilter _filter;
    StabilityDetector _stability;
    Calibration _calibration;

    float _currentWeightGrams = 0.0f;
    int32_t _currentRaw = 0;
    
    CalibState _calibState = CalibState::Idle;
    float _spanRefGrams = 0.0f;
    uint32_t _calibTimeout = 0;

    TaskHandle_t _taskHandle = nullptr;
};

} // namespace Services

#endif // SERVICES_WEIGHT_SERVICE_H
