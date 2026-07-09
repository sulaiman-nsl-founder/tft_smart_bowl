#include "app/ui/SettingsScreen.h"
#include "app/ui/CalibrationScreen.h"
#include "app/ui/OtaScreen.h"
#include "app/ui/DashboardScreen.h"
#include "app/ui/UiManager.h"
#include "app/ui/Theme.h"
#include "drivers/TftDisplay.h"
#include <Arduino.h>
#include <string.h>

namespace App {
namespace Ui {

static const char* MENU_LABELS[] = {
    "Calibration",
    "Update Firmware",
    "Back"
};

SettingsScreen& SettingsScreen::getInstance() {
    static SettingsScreen instance;
    return instance;
}

void SettingsScreen::onEnter() {
    _menuIndex = 0;
    _fullRedraw = true;
}

void SettingsScreen::onExit() {
}

void SettingsScreen::onUpdate() {
    if (_fullRedraw) {
        drawMenu(true);
        _fullRedraw = false;
        _menuRedraw = false;
    } else if (_menuRedraw) {
        drawMenu(false);
        _menuRedraw = false;
    }
}

bool SettingsScreen::onEvent(const Services::SystemEvent& event) {
    if (event.id != Services::EventId::ButtonPress) return false;
    
    uint8_t btn = event.payload.button.buttonId;
    uint8_t type = event.payload.button.eventType;
    
    // Only handle press events (type 1)
    if (type != 1) return false;
    
    if (btn == 0) { // Button 1 = Up
        if (_menuIndex > 0) _menuIndex--;
        _menuRedraw = true;
    } else if (btn == 2) { // Button 3 = Down
        if (_menuIndex < MENU_ITEMS - 1) _menuIndex++;
        _menuRedraw = true;
    } else if (btn == 1) { // Button 2 = Select
        if (_menuIndex == 0) {
            // Selected "Calibration"
            UiManager::getInstance().setScreen(&CalibrationScreen::getInstance());
        } else if (_menuIndex == 1) {
            // Selected "Update Firmware"
            UiManager::getInstance().setScreen(&OtaScreen::getInstance());
        } else {
            // Selected "Back"
            UiManager::getInstance().setScreen(&DashboardScreen::getInstance());
        }
    }
    return true;
}

void SettingsScreen::drawTitleBar(const char* title, uint16_t bgColor, uint16_t textColor) {
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

void SettingsScreen::drawMenu(bool full) {
    auto& tft = Drivers::TftDisplay::getInstance();
    
    if (full) {
        tft.fillScreen(Theme::ColorBackground);
        drawTitleBar("SETTINGS", Theme::ColorAccent, Theme::ColorBackground);
        
        // Draw navigation hint at bottom
        // Dotted separator
        for (int x = 10; x < tft.width() - 10; x += 3) {
            tft.fillRect(x, 110, 1, 1, Theme::ColorTextSecondary);
        }
        
        tft.setTextSize(1);
        tft.setTextColor(Theme::ColorTextSecondary, Theme::ColorBackground);
        tft.setCursor(10, 116);
        tft.print("1:Up 2:OK 3:Down");
    }
    
    // Draw menu items
    for (uint8_t i = 0; i < MENU_ITEMS; i++) {
        uint8_t y = 38 + (i * 24); // Slightly tighter spacing to fit 3 items easily
        
        if (i == _menuIndex) {
            // Selected item: highlighted bar
            tft.fillRect(8, y - 2, tft.width() - 16, 20, Theme::ColorAccent);
            tft.setTextColor(Theme::ColorBackground, Theme::ColorAccent);
        } else {
            // Unselected item
            tft.fillRect(8, y - 2, tft.width() - 16, 20, Theme::ColorBackground);
            tft.setTextColor(Theme::ColorTextPrimary, Theme::ColorBackground);
        }
        
        tft.setTextSize(1);
        tft.setCursor(16, y + 4);
        tft.print(MENU_LABELS[i]);
    }
}

} // namespace Ui
} // namespace App
