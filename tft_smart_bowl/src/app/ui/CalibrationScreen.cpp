#include "app/ui/CalibrationScreen.h"
#include "app/ui/DashboardScreen.h"
#include "app/ui/UiManager.h"
#include "app/ui/Theme.h"
#include "drivers/TftDisplay.h"
#include "services/WeightService.h"
#include <stdio.h>
#include <Arduino.h>

namespace App {
namespace Ui {

// Menu item labels
static const char* MENU_LABELS[] = {
    "Calibration",
    "Back"
};

CalibrationScreen& CalibrationScreen::getInstance() {
    static CalibrationScreen instance;
    return instance;
}

void CalibrationScreen::onEnter() {
    _step = Step::Menu;
    _menuIndex = 0;
    _needsRedraw = true;
}

void CalibrationScreen::onExit() {
    // Nothing to clean up
}

void CalibrationScreen::onUpdate() {
    // Auto-return to dashboard after calibration done
    if (_step == Step::Done && millis() > _doneTimer) {
        UiManager::getInstance().setScreen(&DashboardScreen::getInstance());
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
        if (_step == Step::Menu) {
            drawMenu();
        } else {
            drawStep();
        }
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
        case Step::Menu:
            if (btn == 0) { // Button 1 = Up
                if (_menuIndex > 0) _menuIndex--;
                _needsRedraw = true;
            } else if (btn == 2) { // Button 3 = Down
                if (_menuIndex < MENU_ITEMS - 1) _menuIndex++;
                _needsRedraw = true;
            } else if (btn == 1) { // Button 2 = Select
                if (_menuIndex == 0) {
                    // Selected "Calibration"
                    _step = Step::TarePrompt;
                    _needsRedraw = true;
                } else {
                    // Selected "Back"
                    UiManager::getInstance().setScreen(&DashboardScreen::getInstance());
                }
            }
            return true;
            
        case Step::TarePrompt:
            if (btn == 1) { // Button 2 = OK
                Services::WeightService::getInstance().requestTare();
                _step = Step::Taring;
                _needsRedraw = true;
            }
            return true;
            
        case Step::SpanPrompt:
            if (btn == 1) { // Button 2 = OK
                Services::WeightService::getInstance().requestSpan(100.0f);
                _step = Step::Spanning;
                _needsRedraw = true;
            }
            return true;
            
        case Step::Error:
            if (btn == 1) { // Button 2 = Retry
                _step = Step::TarePrompt;
                _needsRedraw = true;
            }
            return true;
            
        case Step::Done:
            // Any button returns to dashboard
            UiManager::getInstance().setScreen(&DashboardScreen::getInstance());
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

void CalibrationScreen::drawMenu() {
    auto& tft = Drivers::TftDisplay::getInstance();
    tft.fillScreen(Theme::ColorBackground);
    
    drawTitleBar("SETTINGS", Theme::ColorAccent, Theme::ColorBackground);
    
    // Draw menu items
    for (uint8_t i = 0; i < MENU_ITEMS; i++) {
        uint8_t y = 38 + (i * 28);
        
        if (i == _menuIndex) {
            // Selected item: highlighted bar
            tft.fillRect(8, y - 2, tft.width() - 16, 20, Theme::ColorAccent);
            tft.setTextColor(Theme::ColorBackground, Theme::ColorAccent);
        } else {
            tft.setTextColor(Theme::ColorTextPrimary, Theme::ColorBackground);
        }
        
        tft.setTextSize(1);
        tft.setCursor(16, y + 4);
        tft.print(MENU_LABELS[i]);
    }
    
    // Draw navigation hint at bottom
    // Dotted separator
    for (int x = 10; x < tft.width() - 10; x += 3) {
        tft.fillRect(x, 100, 1, 1, Theme::ColorTextSecondary);
    }
    
    tft.setTextSize(1);
    tft.setTextColor(Theme::ColorTextSecondary, Theme::ColorBackground);
    tft.setCursor(10, 110);
    tft.print("1:Up 2:OK 3:Down");
    tft.setTextColor(Theme::ColorTextPrimary, Theme::ColorBackground);
}

void CalibrationScreen::drawStep() {
    auto& tft = Drivers::TftDisplay::getInstance();
    tft.fillScreen(Theme::ColorBackground);
    
    auto& ws = Services::WeightService::getInstance();
    
    switch (_step) {
        case Step::TarePrompt:
            drawTitleBar("STEP 1: TARE", Theme::ColorWarning, Theme::ColorBackground);
            drawCentered("Remove all items", 38, 1, Theme::ColorTextPrimary);
            drawCentered("from the bowl", 52, 1, Theme::ColorTextPrimary);
            
            // Dotted separator
            for (int x = 20; x < tft.width() - 20; x += 3) {
                tft.fillRect(x, 70, 1, 1, Theme::ColorTextSecondary);
            }
            
            drawCentered("Press OK when", 80, 1, Theme::ColorAccent);
            drawCentered("bowl is empty", 94, 1, Theme::ColorAccent);
            
            // Raw value display
            {
                char buf[24];
                snprintf(buf, sizeof(buf), "Raw: %ld", (long)ws.getRawFiltered());
                drawCentered(buf, 114, 1, Theme::ColorTextSecondary);
            }
            break;
            
        case Step::Taring:
            drawTitleBar("TARING...", Theme::ColorWarning, Theme::ColorBackground);
            drawCentered("Please wait", 45, 1, Theme::ColorTextPrimary);
            drawCentered("Stabilizing...", 65, 1, Theme::ColorWarning);
            
            // Show live raw value
            {
                char buf[24];
                snprintf(buf, sizeof(buf), "Raw: %ld", (long)ws.getRawFiltered());
                drawCentered(buf, 95, 1, Theme::ColorTextSecondary);
            }
            break;
            
        case Step::SpanPrompt:
            drawTitleBar("STEP 2: SPAN", Theme::ColorAccent, Theme::ColorBackground);
            drawCentered("Place 100g weight", 38, 1, Theme::ColorTextPrimary);
            drawCentered("on the bowl", 52, 1, Theme::ColorTextPrimary);
            
            // Dotted separator
            for (int x = 20; x < tft.width() - 20; x += 3) {
                tft.fillRect(x, 70, 1, 1, Theme::ColorTextSecondary);
            }
            
            drawCentered("Press OK when", 80, 1, Theme::ColorAccent);
            drawCentered("weight is placed", 94, 1, Theme::ColorAccent);
            
            // Raw value display
            {
                char buf[24];
                snprintf(buf, sizeof(buf), "Raw: %ld", (long)ws.getRawFiltered());
                drawCentered(buf, 114, 1, Theme::ColorTextSecondary);
            }
            break;
            
        case Step::Spanning:
            drawTitleBar("CALIBRATING...", Theme::ColorAccent, Theme::ColorBackground);
            drawCentered("Please wait", 45, 1, Theme::ColorTextPrimary);
            drawCentered("Stabilizing...", 65, 1, Theme::ColorAccent);
            
            // Show live weight
            {
                char buf[24];
                snprintf(buf, sizeof(buf), "%d g   ", (int)ws.getWeight());
                drawCentered(buf, 90, 2, Theme::ColorWarning);
            }
            break;
            
        case Step::Done:
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
            
            // Show final weight reading
            {
                char buf[24];
                snprintf(buf, sizeof(buf), "%d g   ", (int)ws.getWeight());
                drawCentered(buf, 100, 2, Theme::ColorTextPrimary);
            }
            break;
            
        case Step::Error:
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
            break;
            
        default:
            break;
    }
}

} // namespace Ui
} // namespace App
