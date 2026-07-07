#include "app/ui/DashboardScreen.h"
#include "app/ui/Theme.h"
#include "drivers/TftDisplay.h"
#include "services/WeightService.h"
#include <stdio.h>

namespace App {
namespace Ui {

DashboardScreen& DashboardScreen::getInstance() {
    static DashboardScreen instance;
    return instance;
}

void DashboardScreen::onEnter() {
    auto& tft = Drivers::TftDisplay::getInstance();
    tft.clear(Theme::ColorBackground);
    
    // Draw static UI elements
    tft.setCursor(10, 10);
    tft.setTextColor(Theme::ColorTextPrimary);
    tft.setTextSize(2);
    tft.print("Smart Bowl");
    
    tft.fillRect(0, 35, 160, 1, Theme::ColorTextSecondary);
    
    tft.setCursor(10, 50);
    tft.setTextSize(1);
    tft.setTextColor(Theme::ColorTextSecondary);
    tft.print("Current Weight:");
    
    _needsRedraw = true;
}

void DashboardScreen::onExit() {
    // Nothing to clean up
}

void DashboardScreen::onUpdate() {
    if (_needsRedraw) {
        auto& tft = Drivers::TftDisplay::getInstance();
        auto& weightSvc = Services::WeightService::getInstance();

        // Clear the area where the weight is drawn
        tft.fillRect(0, 70, 160, 30, Theme::ColorBackground);
        
        tft.setCursor(10, 70);
        tft.setTextSize(3);
        
        if (weightSvc.isCalibrated()) {
            tft.setTextColor(_lastStable ? Theme::ColorSuccess : Theme::ColorWarning);
            char buf[16];
            snprintf(buf, sizeof(buf), "%.1f g", _lastWeight);
            tft.print(buf);
        } else {
            tft.setTextColor(Theme::ColorError);
            tft.print("UNCAL");
        }
        
        _needsRedraw = false;
    }
}

bool DashboardScreen::onEvent(const Services::SystemEvent& event) {
    if (event.id == Services::EventId::WeightUpdated) {
        if (_lastWeight != event.payload.weight.grams || _lastStable != event.payload.weight.isStable) {
            _lastWeight = event.payload.weight.grams;
            _lastStable = event.payload.weight.isStable;
            _needsRedraw = true;
        }
        return true;
    }
    
    // Pass Tare and Span button presses straight to WeightService for now,
    // though eventually a proper Settings/Calibration screen will handle them.
    if (event.id == Services::EventId::ButtonPress) {
        if (event.payload.button.buttonId == 0 && event.payload.button.eventType == 1) { // 1 = pressed
            Services::WeightService::getInstance().requestTare();
            return true;
        }
        if (event.payload.button.buttonId == 1 && event.payload.button.eventType == 1) {
            Services::WeightService::getInstance().requestSpan(100.0f);
            return true;
        }
    }
    
    return false;
}

} // namespace Ui
} // namespace App
