#include "app/ui/OtaScreen.h"
#include "app/ui/SettingsScreen.h"
#include "app/ui/UiManager.h"
#include "app/ui/Theme.h"
#include "drivers/TftDisplay.h"
#include <Arduino.h>
#include <stdio.h>
#include <string.h>

namespace App {
namespace Ui {

OtaScreen& OtaScreen::getInstance() {
    static OtaScreen instance;
    return instance;
}

void OtaScreen::onEnter() {
    _state = State::Checking;
    _lastState = (State)-1;
    _progressPct = 0;
    _lastUpdateTimer = millis();
    _needsRedraw = true;
    _progressRedraw = false;
}

void OtaScreen::onExit() {
}

void OtaScreen::onUpdate() {
    // Fake OTA simulation logic
    uint32_t now = millis();
    
    if (_state == State::Checking && (now - _lastUpdateTimer > 2000)) {
        _state = State::Downloading;
        _lastUpdateTimer = now;
        _needsRedraw = true;
    } 
    else if (_state == State::Downloading && (now - _lastUpdateTimer > 100)) {
        _progressPct += 2;
        _lastUpdateTimer = now;
        _progressRedraw = true;
        
        if (_progressPct >= 100) {
            _progressPct = 0;
            _state = State::Installing;
            _lastUpdateTimer = now;
            _needsRedraw = true;
        }
    }
    else if (_state == State::Installing && (now - _lastUpdateTimer > 100)) {
        _progressPct += 5;
        _lastUpdateTimer = now;
        _progressRedraw = true;
        
        if (_progressPct >= 100) {
            _state = State::Done;
            _lastUpdateTimer = now;
            _needsRedraw = true;
        }
    }

    if (_needsRedraw || _state != _lastState) {
        auto& tft = Drivers::TftDisplay::getInstance();
        
        bool fullRedraw = (_state != _lastState);
        if (fullRedraw) {
            tft.fillScreen(Theme::ColorBackground);
            _lastState = _state;
        }
        
        switch (_state) {
            case State::Checking:
                if (fullRedraw) drawChecking();
                break;
            case State::Downloading:
                if (fullRedraw) drawProgress("DOWNLOADING...");
                else if (_progressRedraw) drawProgress("DOWNLOADING...");
                break;
            case State::Installing:
                if (fullRedraw) drawProgress("INSTALLING...");
                else if (_progressRedraw) drawProgress("INSTALLING...");
                break;
            case State::Done:
                if (fullRedraw) drawDone();
                break;
            case State::Error:
                if (fullRedraw) drawError();
                break;
        }
        _needsRedraw = false;
        _progressRedraw = false;
    } else if (_progressRedraw) {
        if (_state == State::Downloading) drawProgress("DOWNLOADING...");
        else if (_state == State::Installing) drawProgress("INSTALLING...");
        _progressRedraw = false;
    }
}

bool OtaScreen::onEvent(const Services::SystemEvent& event) {
    if (event.id != Services::EventId::ButtonPress) return false;
    
    uint8_t btn = event.payload.button.buttonId;
    uint8_t type = event.payload.button.eventType;
    
    // Only handle press events (type 1)
    if (type != 1) return false;
    
    // Allow cancellation during Checking or Error/Done states
    if (_state == State::Checking || _state == State::Done || _state == State::Error) {
        if (btn == 2 || btn == 1) { // OK or Cancel
            UiManager::getInstance().setScreen(&SettingsScreen::getInstance());
            return true;
        }
    }
    
    return true; // Consume all inputs while OTA is running
}

void OtaScreen::drawTitleBar(const char* title, uint16_t bgColor, uint16_t textColor) {
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

void OtaScreen::drawCentered(const char* text, uint8_t y, uint8_t size, uint16_t color) {
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

void OtaScreen::drawChecking() {
    drawTitleBar("OTA UPDATE", Theme::ColorAccent, Theme::ColorBackground);
    drawCentered("Checking for", 45, 1, Theme::ColorTextPrimary);
    drawCentered("new firmware...", 65, 1, Theme::ColorTextPrimary);
    
    drawCentered("Press 3 to Cancel", 100, 1, Theme::ColorTextSecondary);
}

void OtaScreen::drawProgress(const char* label) {
    drawTitleBar(label, Theme::ColorWarning, Theme::ColorBackground);
    
    auto& tft = Drivers::TftDisplay::getInstance();
    
    // Progress bar outline
    int barW = tft.width() - 40;
    int barH = 14;
    int barX = 20;
    int barY = 55;
    tft.drawRect(barX, barY, barW, barH, Theme::ColorTextSecondary);
    
    // Progress fill
    int fillW = (barW - 4) * _progressPct / 100;
    tft.fillRect(barX + 2, barY + 2, fillW, barH - 4, Theme::ColorAccent);
    
    // Percentage text
    char buf[16];
    snprintf(buf, sizeof(buf), "%3d%%", _progressPct);
    drawCentered(buf, 80, 1, Theme::ColorTextPrimary);
    
    drawCentered("Do not power off!", 105, 1, Theme::ColorError);
}

void OtaScreen::drawDone() {
    drawTitleBar("UPDATE COMPLETE", Theme::ColorSuccess, Theme::ColorBackground);
    
    auto& tft = Drivers::TftDisplay::getInstance();
    int cx = tft.width() / 2;
    int cy = 55;
    for (int i = 0; i < 5; i++) {
        tft.fillRect(cx - 8 + i, cy - 2 + i, 2, 2, Theme::ColorSuccess);
    }
    for (int i = 0; i < 9; i++) {
        tft.fillRect(cx - 3 + i, cy + 2 - i, 2, 2, Theme::ColorSuccess);
    }
    
    drawCentered("Success!", 80, 1, Theme::ColorSuccess);
    drawCentered("Press any to return", 100, 1, Theme::ColorTextSecondary);
}

void OtaScreen::drawError() {
    drawTitleBar("UPDATE FAILED", Theme::ColorError, Theme::ColorTextPrimary);
    drawCentered("Network Error", 50, 1, Theme::ColorError);
    drawCentered("Press any to return", 90, 1, Theme::ColorTextSecondary);
}

} // namespace Ui
} // namespace App
