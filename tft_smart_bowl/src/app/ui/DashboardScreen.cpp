#include "app/ui/DashboardScreen.h"
#include "app/ui/Theme.h"
#include "drivers/TftDisplay.h"
#include "services/WeightService.h"
#include "services/ProvisioningService.h"
#include "services/TimeService.h"
#include "app/ui/ProvisioningScreen.h"
#include "app/ui/UiManager.h"
#include <stdio.h>
#include <time.h>

namespace App {
namespace Ui {

DashboardScreen& DashboardScreen::getInstance() {
    static DashboardScreen instance;
    return instance;
}

void DashboardScreen::onEnter() {
    _wasProvisioned = Services::ProvisioningService::getInstance().isProvisioned();
    _wifiNeedsRedraw = true;
    _lastMinute = 60; // Force initial time redraw
    auto& tft = Drivers::TftDisplay::getInstance();
    tft.clear(Theme::ColorBackground);
    
    // Draw title bar with accent separator
    tft.fillRect(0, 0, 160, 22, Theme::ColorAccent);
    tft.fillRect(0, 22, 160, 2, Theme::ColorWarning); // subtle accent line
    
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
    auto& tft = Drivers::TftDisplay::getInstance();

    // Check time updates
    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    
    if (timeinfo.tm_min != _lastMinute) {
        _lastMinute = timeinfo.tm_min;
        
        auto& timeSvc = Services::TimeService::getInstance();
        
        // Clear top-left (Date) and top-right (Time)
        tft.fillRect(5, 5, 80, 16, Theme::ColorAccent);  // Date area
        tft.fillRect(90, 5, 42, 16, Theme::ColorAccent); // Time area (before WiFi icon at 135)
        
        tft.setTextSize(1);
        tft.setTextColor(Theme::ColorBackground, Theme::ColorAccent);
        
        // Draw Date
        tft.setCursor(5, 5);
        tft.print(timeSvc.getFormattedDate().c_str());
        
        // Draw Time
        tft.setCursor(100, 5);
        tft.print(timeSvc.getFormattedTime().c_str());
    }

    if (_wifiNeedsRedraw) {
        // Draw WiFi Icon on top right
        // Clear the icon area first
        tft.fillRect(135, 5, 20, 16, Theme::ColorAccent);
        if (_wasProvisioned) {
            tft.fillRect(140, 14, 2, 4, Theme::ColorSuccess);
            tft.fillRect(144, 10, 2, 8, Theme::ColorSuccess);
            tft.fillRect(148, 6, 2, 12, Theme::ColorSuccess);
        } else {
            // Draw empty bars
            tft.fillRect(140, 14, 2, 4, Theme::ColorTextSecondary);
            tft.fillRect(144, 10, 2, 8, Theme::ColorTextSecondary);
            tft.fillRect(148, 6, 2, 12, Theme::ColorTextSecondary);
            // Draw a red line across it to indicate disconnected
            tft.fillRect(138, 12, 14, 2, Theme::ColorError);
        }
        _wifiNeedsRedraw = false;
    }

    if (_needsRedraw) {
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
    if (event.id == Services::EventId::ProvisioningStateChanged) {
        bool newProv = Services::ProvisioningService::getInstance().isProvisioned();
        if (_wasProvisioned != newProv) {
            _wasProvisioned = newProv;
            _wifiNeedsRedraw = true;
        }
        return true;
    }
    
    if (event.id == Services::EventId::ButtonPress) {
        if (event.payload.button.buttonId == 1 && event.payload.button.eventType == 1) { // 1 = middle button, pressed
            if (!_wasProvisioned) {
                UiManager::getInstance().setScreen(&ProvisioningScreen::getInstance());
            } else {
                Services::WeightService::getInstance().requestTare();
            }
            return true;
        }
    }
    
    return false;
}

} // namespace Ui
} // namespace App
