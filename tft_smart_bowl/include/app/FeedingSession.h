#ifndef APP_FEEDING_SESSION_H
#define APP_FEEDING_SESSION_H

#include "app/BowlStateMachine.h"
#include "services/EventBus.h"
#include <stdint.h>

namespace App {

/**
 * @brief Represents a discrete eating event.
 */
class FeedingSession {
public:
    static FeedingSession& getInstance();

    void begin();

private:
    FeedingSession() = default;
    ~FeedingSession() = default;
    FeedingSession(const FeedingSession&) = delete;
    FeedingSession& operator=(const FeedingSession&) = delete;

    void handleBowlStateChange(BowlState oldState, BowlState newState);
    void handleWeightUpdate(float grams, bool isStable);

    bool _isActive = false;
    uint32_t _startTimeMs = 0;
    float _initialGrams = 0.0f;
    float _currentGrams = 0.0f;
    float _consumedGrams = 0.0f;
    
    // To generate a simple unique ID for this session
    uint32_t _sessionId = 0;
};

} // namespace App

#endif // APP_FEEDING_SESSION_H
