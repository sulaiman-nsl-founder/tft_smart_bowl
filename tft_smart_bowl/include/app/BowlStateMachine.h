#ifndef APP_BOWL_STATE_MACHINE_H
#define APP_BOWL_STATE_MACHINE_H

#include "services/EventBus.h"
#include <stdint.h>

namespace App {

/**
 * @brief High-level states of the physical bowl.
 */
enum class BowlState : uint8_t {
    Unavailable = 0, // Scale is uncalibrated or faulted
    Empty,           // Bowl is empty (~0g)
    FoodPresent,     // Food has been added and is sitting stably
    Consuming,       // Weight is actively decreasing / unstable (pet is eating)
    Leftover,        // Eating stopped, some food remains
    Aborted          // Bowl was removed entirely (large negative weight)
};

/**
 * @brief Tracks the semantic state of the bowl based on weight events.
 * 
 * Uses hysteresis and stability to transition between states.
 */
class BowlStateMachine {
public:
    static BowlStateMachine& getInstance();

    /**
     * @brief Initialize the state machine.
     */
    void begin();

    /**
     * @brief Must be called periodically or upon receiving a WeightUpdated event.
     * @param weightGrams The current live weight.
     * @param isStable Whether the scale is currently stable.
     */
    void processWeightUpdate(float weightGrams, bool isStable);

    /**
     * @brief Get the current semantic state of the bowl.
     */
    BowlState getState() const;

private:
    BowlStateMachine() = default;
    ~BowlStateMachine() = default;
    BowlStateMachine(const BowlStateMachine&) = delete;
    BowlStateMachine& operator=(const BowlStateMachine&) = delete;

    BowlState _currentState = BowlState::Unavailable;
    
    float _stableWeight = 0.0f;
    uint32_t _stateEnterTime = 0;

    void changeState(BowlState newState);
};

} // namespace App

#endif // APP_BOWL_STATE_MACHINE_H
