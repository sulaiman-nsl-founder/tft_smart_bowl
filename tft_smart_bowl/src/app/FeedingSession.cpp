#include "app/FeedingSession.h"
#include "services/Logger.h"
#include <Arduino.h>

namespace App {

FeedingSession& FeedingSession::getInstance() {
    static FeedingSession instance;
    return instance;
}

void FeedingSession::begin() {
    Services::EventBus::getInstance().subscribe([](const Services::SystemEvent& event) {
        auto& session = FeedingSession::getInstance();
        
        if (event.id == Services::EventId::BowlStateChanged) {
            session.handleBowlStateChange(
                static_cast<BowlState>(event.payload.bowlState.oldState),
                static_cast<BowlState>(event.payload.bowlState.newState)
            );
        } else if (event.id == Services::EventId::WeightUpdated) {
            session.handleWeightUpdate(
                event.payload.weight.grams,
                event.payload.weight.isStable
            );
        }
    });
}

void FeedingSession::handleBowlStateChange(BowlState oldState, BowlState newState) {
    if (newState == BowlState::Consuming && !_isActive) {
        // Start a new feeding session
        _isActive = true;
        _startTimeMs = millis();
        _sessionId++; // Simple incrementing ID for now
        _initialGrams = _currentGrams;
        _consumedGrams = 0.0f;
        
        LOG_INFO("FEED", 1000, "Feeding session %u STARTED. Initial: %.1fg", 
                 _sessionId, _initialGrams);
                 
        Services::SystemEvent evt;
        evt.id = Services::EventId::FeedingSessionStarted;
        evt.timestampMs = millis();
        evt.payload.feeding.amountGrams = 0.0f;
        Services::EventBus::getInstance().publish(evt);
        
    } else if (_isActive && (newState == BowlState::Leftover || newState == BowlState::Empty || newState == BowlState::Aborted)) {
        // End the feeding session
        _isActive = false;
        
        // Calculate total consumed (ensure it doesn't go negative)
        float consumed = _initialGrams - _currentGrams;
        if (consumed < 0.0f) consumed = 0.0f;
        
        uint32_t duration = (millis() - _startTimeMs) / 1000; // in seconds
        
        LOG_INFO("FEED", 1001, "Feeding session %u ENDED. Consumed: %.1fg in %us", 
                 _sessionId, consumed, duration);
                 
        Services::SystemEvent evt;
        evt.id = Services::EventId::FeedingSessionEnded;
        evt.timestampMs = millis();
        evt.payload.feeding.amountGrams = consumed;
        Services::EventBus::getInstance().publish(evt);
    }
}

void FeedingSession::handleWeightUpdate(float grams, bool isStable) {
    _currentGrams = grams;
    
    // Update live consumption if active
    if (_isActive) {
        float consumed = _initialGrams - _currentGrams;
        if (consumed < 0.0f) consumed = 0.0f;
        _consumedGrams = consumed;
    }
}

} // namespace App
