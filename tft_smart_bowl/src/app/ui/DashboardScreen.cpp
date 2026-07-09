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
    
    // Draw title bar with accent separator
    tft.fillRect(0, 0, 160, 22, Theme::ColorAccent);
    tft.fillRect(0, 22, 160, 2, Theme::ColorWarning); // subtle accent line
    tft.setCursor(10, 5);
    tft.setTextColor(Theme::ColorBackground, Theme::ColorAccent);
    tft.setTextSize(2);
    tft.print("SMART BOWL");
    
    // Dotted separator
    tft.setTextColor(Theme::ColorTextSecondary, Theme::ColorBackground);
    for (int x = 20; x < 140; x += 3) {
        tft.fillRect(x, 38, 1, 1, Theme::ColorTextSecondary);
    }
    
    // Label
    tft.setCursor(10, 44);
    tft.setTextSize(1);
    tft.setTextColor(Theme::ColorTextSecondary, Theme::ColorBackground);
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

        // Use fixed width or trailing spaces to avoid leaving artifacts when number shrinks
        // and remove the fillRect(0, 58, ...) that causes full box flicker
        
        tft.setCursor(10, 62);
        tft.setTextSize(3);
        
        if (weightSvc.isCalibrated()) {
            tft.setTextColor(_lastStable ? Theme::ColorSuccess : Theme::ColorWarning, Theme::ColorBackground);
            char buf[16];
            snprintf(buf, sizeof(buf), "%d g   ", (int)_lastWeight);
            tft.print(buf);
        } else {
            tft.setTextColor(Theme::ColorError, Theme::ColorBackground);
            tft.print("UNCAL");
        }
        
        // Status indicator at bottom
        tft.fillRect(0, 110, 160, 18, Theme::ColorBackground);
        for (int x = 20; x < 140; x += 3) {
            tft.fillRect(x, 108, 1, 1, Theme::ColorTextSecondary);
        }
        tft.setTextSize(1);
        tft.setTextColor(_lastStable ? Theme::ColorSuccess : Theme::ColorTextSecondary, Theme::ColorBackground);
        tft.setCursor(10, 114);
        tft.print(_lastStable ? "Stable" : "Measuring...");
        
        _needsRedraw = false;
    }
}

bool DashboardScreen::onEvent(const Services::SystemEvent& event) {
    if (event.id == Services::EventId::WeightUpdated) {
        int newIntWeight = (int)event.payload.weight.grams;
        bool newStable = event.payload.weight.isStable;
        
        if ((int)_lastWeight != newIntWeight || _lastStable != newStable) {
            _lastWeight = event.payload.weight.grams; // store the real value
            _lastStable = newStable;
            _needsRedraw = true;
        }
        return true;
    }
    
    if (event.id == Services::EventId::ButtonPress) {
        if (event.payload.button.buttonId == 1 && event.payload.button.eventType == 1) { // 1 = middle button, pressed
            Services::WeightService::getInstance().requestTare();
            return true;
        }
    }
    
    return false;
}

} // namespace Ui
} // namespace App
