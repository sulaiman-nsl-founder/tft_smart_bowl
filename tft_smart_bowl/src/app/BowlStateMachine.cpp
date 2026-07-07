#include "app/BowlStateMachine.h"
#include "services/Logger.h"
#include <Arduino.h>

namespace App {

static constexpr float THRESHOLD_EMPTY_G = 3.0f;     // Below this is Empty
static constexpr float THRESHOLD_ABORTED_G = -10.0f; // Bowl removed
static constexpr float CONSUMING_DROP_G = 2.0f;      // Drop required to enter Consuming

BowlStateMachine& BowlStateMachine::getInstance() {
    static BowlStateMachine instance;
    return instance;
}

void BowlStateMachine::begin() {
    _currentState = BowlState::Unavailable;
    _stateEnterTime = millis();

    Services::EventBus::getInstance().subscribe([](const Services::SystemEvent& event) {
        if (event.id == Services::EventId::WeightUpdated) {
            BowlStateMachine::getInstance().processWeightUpdate(
                event.payload.weight.grams,
                event.payload.weight.isStable
            );
        }
    });
}

void BowlStateMachine::changeState(BowlState newState) {
    if (_currentState != newState) {
        LOG_INFO("BOWL", 900, "Bowl State Change: %u -> %u", 
                 static_cast<uint8_t>(_currentState), 
                 static_cast<uint8_t>(newState));
        
        Services::SystemEvent evt;
        evt.id = Services::EventId::BowlStateChanged;
        evt.timestampMs = millis();
        evt.payload.bowlState.oldState = static_cast<uint8_t>(_currentState);
        evt.payload.bowlState.newState = static_cast<uint8_t>(newState);
        Services::EventBus::getInstance().publish(evt);

        _currentState = newState;
        _stateEnterTime = millis();
    }
}

void BowlStateMachine::processWeightUpdate(float weightGrams, bool isStable) {
    // If we're not calibrated (caller should pass this logically, but we'll 
    // assume for now that if weight is exactly 0 and never changes, it might be uncalibrated. 
    // Let's rely on WeightService to report if calibrated. For now, we just use weight.)
    
    // Core state transitions
    switch (_currentState) {
        case BowlState::Unavailable:
            // Boot state. If we get a stable reading, figure out where we are.
            if (isStable) {
                if (weightGrams <= THRESHOLD_ABORTED_G) {
                    changeState(BowlState::Aborted);
                } else if (weightGrams <= THRESHOLD_EMPTY_G) {
                    changeState(BowlState::Empty);
                } else {
                    _stableWeight = weightGrams;
                    changeState(BowlState::FoodPresent);
                }
            }
            break;

        case BowlState::Empty:
            if (weightGrams <= THRESHOLD_ABORTED_G) {
                changeState(BowlState::Aborted);
            } else if (isStable && weightGrams > THRESHOLD_EMPTY_G) {
                _stableWeight = weightGrams;
                changeState(BowlState::FoodPresent);
            }
            break;

        case BowlState::FoodPresent:
        case BowlState::Leftover:
            if (weightGrams <= THRESHOLD_ABORTED_G) {
                changeState(BowlState::Aborted);
            } else if (isStable && weightGrams <= THRESHOLD_EMPTY_G) {
                changeState(BowlState::Empty);
            } else if (!isStable && weightGrams < (_stableWeight - CONSUMING_DROP_G)) {
                // Weight is dropping and unstable -> eating
                changeState(BowlState::Consuming);
            } else if (isStable && weightGrams > _stableWeight + 5.0f) {
                // More food added
                _stableWeight = weightGrams;
                changeState(BowlState::FoodPresent);
            }
            break;

        case BowlState::Consuming:
            if (weightGrams <= THRESHOLD_ABORTED_G) {
                changeState(BowlState::Aborted);
            } else if (isStable) {
                if (weightGrams <= THRESHOLD_EMPTY_G) {
                    changeState(BowlState::Empty);
                } else {
                    _stableWeight = weightGrams;
                    changeState(BowlState::Leftover);
                }
            }
            break;

        case BowlState::Aborted:
            if (isStable && weightGrams > THRESHOLD_ABORTED_G) {
                if (weightGrams <= THRESHOLD_EMPTY_G) {
                    changeState(BowlState::Empty);
                } else {
                    _stableWeight = weightGrams;
                    changeState(BowlState::FoodPresent);
                }
            }
            break;
    }
}

BowlState BowlStateMachine::getState() const {
    return _currentState;
}

} // namespace App
