#include "app/ui/CalibrationScreen.h"
#include "app/ui/SettingsScreen.h"
#include "app/ui/DashboardScreen.h"
#include "app/ui/UiManager.h"
#include "app/ui/Theme.h"
#include "drivers/TftDisplay.h"
#include "services/WeightService.h"
#include <stdio.h>
#include <Arduino.h>

namespace App {
namespace Ui {

CalibrationScreen& CalibrationScreen::getInstance() {
    static CalibrationScreen instance;
    return instance;
}

void CalibrationScreen::onEnter() {
    _step = Step::TarePrompt;
    _lastStep = (Step)-1; // Force full redraw
    _needsRedraw = true;
}

void CalibrationScreen::onExit() {
    // Nothing to clean up
}

void CalibrationScreen::onUpdate() {
    // Auto-return to dashboard after calibration done
    if (_step == Step::Done && millis() > _doneTimer) {
        UiManager::getInstance().setScreen(&SettingsScreen::getInstance());
        return;
    }
    
    // Monitor calibration state changes during Taring/Spanning
    if (_step == Step::Taring || _step == Step::Spanning) {
        auto state = Services::WeightService::getInstance().getCalibState();
        
        if (state == Services::WeightService::CalibState::Idle) {
            // Calibration step finished
            if (_step == Step::Taring) {
                // Tare done, move to span prompt
                _step = Step::SpanPrompt;
                _needsRedraw = true;
            } else {
                // Span done, calibration complete!
                _step = Step::Done;
                _doneTimer = millis() + 3000; // Auto-return after 3 seconds
                _needsRedraw = true;
            }
        }
        
        // Refresh display to show live values
        _needsRedraw = true;
    }
    
    if (_needsRedraw) {
        drawStep();
        _needsRedraw = false;
    }
}

bool CalibrationScreen::onEvent(const Services::SystemEvent& event) {
    if (event.id != Services::EventId::ButtonPress) return false;
    
    uint8_t btn = event.payload.button.buttonId;
    uint8_t type = event.payload.button.eventType;
    
    // Only handle press events (type 1)
    if (type != 1) return false;
    
    switch (_step) {
        case Step::TarePrompt:
            if (btn == 1) { // Button 2 = OK
                Services::WeightService::getInstance().requestTare();
                _step = Step::Taring;
                _needsRedraw = true;
            } else if (btn == 2) { // Button 3 = Cancel
                UiManager::getInstance().setScreen(&SettingsScreen::getInstance());
            }
            return true;
            
        case Step::SpanPrompt:
            if (btn == 1) { // Button 2 = OK
                Services::WeightService::getInstance().requestSpan(100.0f);
                _step = Step::Spanning;
                _needsRedraw = true;
            } else if (btn == 2) { // Button 3 = Cancel
                UiManager::getInstance().setScreen(&SettingsScreen::getInstance());
            }
            return true;
            
        case Step::Error:
            if (btn == 1) { // Button 2 = Retry
                _step = Step::TarePrompt;
                _needsRedraw = true;
            } else if (btn == 2) { // Button 3 = Cancel
                UiManager::getInstance().setScreen(&SettingsScreen::getInstance());
            }
            return true;
            
        case Step::Done:
            // Any button returns to settings
            UiManager::getInstance().setScreen(&SettingsScreen::getInstance());
            return true;
            
        default:
            break;
    }
    
    return false;
}

// ---- Drawing Helpers ----

void CalibrationScreen::drawTitleBar(const char* title, uint16_t bgColor, uint16_t textColor) {
    auto& tft = Drivers::TftDisplay::getInstance();
    tft.fillRect(0, 0, tft.width(), 22, bgColor);
    tft.fillRect(0, 22, tft.width(), 2, Theme::ColorAccent);
    tft.setTextColor(textColor, bgColor);
    tft.setTextSize(1);
    
    int len = strlen(title);
    int w = len * 6;
    int x = (tft.width() - w) / 2;
    if (x < 0) x = 0;
    
    tft.setCursor(x, 7);
    tft.print(title);
    tft.setTextColor(Theme::ColorTextPrimary, Theme::ColorBackground);
}

void CalibrationScreen::drawCentered(const char* text, uint8_t y, uint8_t size, uint16_t color) {
    auto& tft = Drivers::TftDisplay::getInstance();
    tft.setTextSize(size);
    tft.setTextColor(color, Theme::ColorBackground);
    
    int len = strlen(text);
    int w = len * 6 * size;
    int x = (tft.width() - w) / 2;
    if (x < 0) x = 0;
    
    tft.setCursor(x, y);
    tft.print(text);
    tft.setTextColor(Theme::ColorTextPrimary, Theme::ColorBackground);
}



void CalibrationScreen::drawStep() {
    auto& tft = Drivers::TftDisplay::getInstance();
    
    bool fullRedraw = (_step != _lastStep);
    if (fullRedraw) {
        tft.fillScreen(Theme::ColorBackground);
        _lastStep = _step;
    }
    
    auto& ws = Services::WeightService::getInstance();
    
    switch (_step) {
        case Step::TarePrompt:
            if (fullRedraw) {
                drawTitleBar("STEP 1: TARE", Theme::ColorWarning, Theme::ColorBackground);
                drawCentered("Remove all items", 38, 1, Theme::ColorTextPrimary);
                drawCentered("from the bowl", 52, 1, Theme::ColorTextPrimary);
                
                // Dotted separator
                for (int x = 20; x < tft.width() - 20; x += 3) {
                    tft.fillRect(x, 70, 1, 1, Theme::ColorTextSecondary);
                }
                
                drawCentered("Press 2: OK", 80, 1, Theme::ColorAccent);
                drawCentered("Press 3: Cancel", 94, 1, Theme::ColorTextSecondary);
            }            
            // Dotted separator
            for (int x = 20; x < tft.width() - 20; x += 3) {
                tft.fillRect(x, 70, 1, 1, Theme::ColorTextSecondary);
            }
            
            drawCentered("Press 2: OK", 80, 1, Theme::ColorAccent);
            drawCentered("Press 3: Cancel", 94, 1, Theme::ColorTextSecondary);
            
            // Raw value display
            {
                char buf[32];
                snprintf(buf, sizeof(buf), "Raw: %-8ld  ", (long)ws.getRawFiltered());
                drawCentered(buf, 114, 1, Theme::ColorTextSecondary);
            }
            break;
            
        case Step::Taring:
            if (fullRedraw) {
                drawTitleBar("TARING...", Theme::ColorWarning, Theme::ColorBackground);
                drawCentered("Please wait", 45, 1, Theme::ColorTextPrimary);
                drawCentered("Stabilizing...", 65, 1, Theme::ColorWarning);
            }
            
            // Show live raw value
            {
                char buf[32];
                snprintf(buf, sizeof(buf), "Raw: %-8ld  ", (long)ws.getRawFiltered());
                drawCentered(buf, 95, 1, Theme::ColorTextSecondary);
            }
            break;
            
        case Step::SpanPrompt:
            if (fullRedraw) {
                drawTitleBar("STEP 2: SPAN", Theme::ColorAccent, Theme::ColorBackground);
                drawCentered("Place 100g weight", 38, 1, Theme::ColorTextPrimary);
                drawCentered("on the bowl", 52, 1, Theme::ColorTextPrimary);
                
                // Dotted separator
                for (int x = 20; x < tft.width() - 20; x += 3) {
                    tft.fillRect(x, 70, 1, 1, Theme::ColorTextSecondary);
                }
                
                drawCentered("Press 2: OK", 80, 1, Theme::ColorAccent);
                drawCentered("Press 3: Cancel", 94, 1, Theme::ColorTextSecondary);
            }
            
            // Raw value display
            {
                char buf[32];
                snprintf(buf, sizeof(buf), "Raw: %-8ld  ", (long)ws.getRawFiltered());
                drawCentered(buf, 114, 1, Theme::ColorTextSecondary);
            }
            break;
            
        case Step::Spanning:
            if (fullRedraw) {
                drawTitleBar("CALIBRATING...", Theme::ColorAccent, Theme::ColorBackground);
                drawCentered("Please wait", 45, 1, Theme::ColorTextPrimary);
                drawCentered("Stabilizing...", 65, 1, Theme::ColorAccent);
            }
            
            // Show live weight
            {
                char buf[24];
                snprintf(buf, sizeof(buf), "%5d g   ", (int)ws.getWeight());
                drawCentered(buf, 90, 2, Theme::ColorWarning);
            }
            break;
            
        case Step::Done:
            if (fullRedraw) {
                drawTitleBar("COMPLETE!", Theme::ColorSuccess, Theme::ColorBackground);
                
                // Checkmark icon
                {
                    int cx = tft.width() / 2;
                    int cy = 46;
                    for (int i = 0; i < 5; i++) {
                        tft.fillRect(cx - 8 + i, cy - 2 + i, 2, 2, Theme::ColorSuccess);
                    }
                    for (int i = 0; i < 9; i++) {
                        tft.fillRect(cx - 3 + i, cy + 2 - i, 2, 2, Theme::ColorSuccess);
                    }
                }
                
                drawCentered("Calibration", 64, 1, Theme::ColorSuccess);
                drawCentered("Saved!", 78, 1, Theme::ColorSuccess);
            }
            
            // Show final weight reading
            {
                char buf[24];
                snprintf(buf, sizeof(buf), "%5d g   ", (int)ws.getWeight());
                drawCentered(buf, 100, 2, Theme::ColorTextPrimary);
            }
            break;
            
        case Step::Error:
            if (fullRedraw) {
                drawTitleBar("ERROR", Theme::ColorError, Theme::ColorTextPrimary);
                
                // X icon
                {
                    int cx = tft.width() / 2;
                    int cy = 48;
                    for (int i = 0; i < 15; i++) {
                        tft.fillRect(cx - 7 + i, cy - 7 + i, 2, 2, Theme::ColorError);
                        tft.fillRect(cx + 7 - i, cy - 7 + i, 2, 2, Theme::ColorError);
                    }
                }
                
                drawCentered("Scale not stable", 72, 1, Theme::ColorError);
                drawCentered("Press OK to retry", 96, 1, Theme::ColorTextSecondary);
            }
            break;
            
        default:
            break;
    }
}

} // namespace Ui
} // namespace App
