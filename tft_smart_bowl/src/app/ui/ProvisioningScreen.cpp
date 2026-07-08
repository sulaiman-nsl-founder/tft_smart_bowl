#include "app/ui/ProvisioningScreen.h"
#include "app/ui/DashboardScreen.h"
#include "app/ui/UiManager.h"
#include "app/ui/Theme.h"
#include "drivers/TftDisplay.h"
#include "platform/BuildInfo.h"
#include <WiFi.h>
#include "qrcode.h"

namespace App {
namespace Ui {

static const char* POP_PIN = "1234";
static const char* SERVICE_UUID = "12345678-1234-1234-1234-123456789abc";

ProvisioningScreen& ProvisioningScreen::getInstance() {
    static ProvisioningScreen instance;
    return instance;
}

void ProvisioningScreen::onEnter() {
    _deviceMAC = WiFi.macAddress();
    _currentState = ProvisioningState::Boot;
    _needsRedraw = true;
}

void ProvisioningScreen::onExit() {
    // Clean up if needed
}

void ProvisioningScreen::onUpdate() {
    if (_needsRedraw) {
        renderScreen(_currentState);
        _needsRedraw = false;
    }
}

bool ProvisioningScreen::onEvent(const Services::SystemEvent& event) {
    if (event.id == Services::EventId::ProvisioningStateChanged) {
        ProvisioningState newState = static_cast<ProvisioningState>(event.payload.provisioning.state);
        
        // Handle IP string format
        if (newState == ProvisioningState::WifiConnected || newState == ProvisioningState::AutoConnected) {
            char ipBuf[16];
            snprintf(ipBuf, sizeof(ipBuf), "%u.%u.%u.%u", 
                event.payload.provisioning.ip[0],
                event.payload.provisioning.ip[1],
                event.payload.provisioning.ip[2],
                event.payload.provisioning.ip[3]);
            _connectedIP = ipBuf;
        }

        if (_currentState != newState) {
            _currentState = newState;
            _needsRedraw = true;
            
            if (newState == ProvisioningState::WifiConnected || newState == ProvisioningState::AutoConnected) {
                // Transition to the main dashboard after a short delay
                // Note: In a real system, we might want a timer, but this will do for now.
                UiManager::getInstance().setScreen(&DashboardScreen::getInstance());
            }
        }
        return true;
    }
    return false;
}

static void qrDisplayCallback(esp_qrcode_handle_t qrcode) {
    uint8_t size = esp_qrcode_get_size(qrcode);
    auto& tft = Drivers::TftDisplay::getInstance();
    
    // Title bar height
    const uint8_t titleH = 20;
    
    // Scale QR to fit below the title bar, centered on the screen
    uint8_t availH = tft.height() - titleH - 4; // leave a small bottom margin
    uint8_t scale = availH / size;
    if (scale < 1) scale = 1;
    uint8_t qrW = size * scale;
    uint8_t marginX = (tft.width() - qrW) / 2;
    uint8_t marginY = titleH + (availH - qrW) / 2;

    // Draw a solid white box behind the QR code
    tft.fillRect(marginX - 4, marginY - 4, qrW + 8, qrW + 8, Drivers::TftDisplay::COLOR_WHITE);

    // Draw black modules
    for (uint8_t y = 0; y < size; y++) {
        for (uint8_t x = 0; x < size; x++) {
            if (esp_qrcode_get_module(qrcode, x, y)) {
                tft.fillRect(marginX + (x * scale), marginY + (y * scale), scale, scale, Drivers::TftDisplay::COLOR_BLACK);
            }
        }
    }
}

void ProvisioningScreen::drawQRScreen() {
    auto& tft = Drivers::TftDisplay::getInstance();
    tft.fillScreen(Drivers::TftDisplay::COLOR_BLACK);
    
    // Draw title bar: "Scan to connect with app"
    drawTitleBar("Scan to connect with app", Theme::ColorAccent, Theme::ColorBackground);
    
    String qrContent = String("W:") + SERVICE_UUID + ":" + POP_PIN;
    
    esp_qrcode_config_t cfg = {
        .display_func       = qrDisplayCallback,
        .max_qrcode_version = 10,
        .qrcode_ecc_level   = ESP_QRCODE_ECC_LOW,
    };
    esp_qrcode_generate(&cfg, qrContent.c_str());
}

void ProvisioningScreen::drawTitleBar(const char* title, uint16_t bgColor, uint16_t textColor) {
    auto& tft = Drivers::TftDisplay::getInstance();
    // Taller title bar with a subtle accent line at the bottom
    tft.fillRect(0, 0, tft.width(), 22, bgColor);
    tft.fillRect(0, 22, tft.width(), 2, Theme::ColorAccent); // accent separator
    tft.setTextColor(textColor, bgColor);
    tft.setTextSize(1);
    
    int len = strlen(title);
    int charWidth = 6;
    int w = len * charWidth;
    int x = (tft.width() - w) / 2;
    if (x < 0) x = 0;
    
    tft.setCursor(x, 7);
    tft.print(title);
    tft.setTextColor(Theme::ColorTextPrimary, Theme::ColorBackground);
}

void ProvisioningScreen::drawCentered(const char* text, uint8_t y, uint8_t size, uint16_t color) {
    auto& tft = Drivers::TftDisplay::getInstance();
    tft.setTextSize(size);
    tft.setTextColor(color, Theme::ColorBackground);
    
    int len = strlen(text);
    int charWidth = 6 * size;
    int w = len * charWidth;
    int x = (tft.width() - w) / 2;
    if (x < 0) x = 0;
    
    tft.setCursor(x, y);
    tft.print(text);
    tft.setTextColor(Theme::ColorTextPrimary, Theme::ColorBackground);
}

void ProvisioningScreen::drawWrapped(const char* label, const String& value, uint8_t y) {
    auto& tft = Drivers::TftDisplay::getInstance();
    tft.setTextSize(1);
    tft.setTextColor(Theme::ColorTextSecondary, Theme::ColorBackground);
    tft.setCursor(10, y);
    tft.print(label);
    tft.setTextColor(Theme::ColorAccent, Theme::ColorBackground);
    tft.setCursor(10, y + 14);
    tft.print(value.substring(0, 22).c_str());
    tft.setTextColor(Theme::ColorTextPrimary, Theme::ColorBackground);
}

// Helper: draw a simple checkmark icon centered at (cx, cy) using fillRect
static void drawCheckIcon(Drivers::TftDisplay& tft, int cx, int cy, uint16_t color) {
    // Checkmark: short leg going down-right, then long leg going up-right
    // Short leg (4 pixels diagonal)
    for (int i = 0; i < 5; i++) {
        tft.fillRect(cx - 8 + i, cy - 2 + i, 2, 2, color);
    }
    // Long leg (8 pixels diagonal going up-right)
    for (int i = 0; i < 9; i++) {
        tft.fillRect(cx - 3 + i, cy + 2 - i, 2, 2, color);
    }
}

// Helper: draw a simple X icon centered at (cx, cy) using fillRect
static void drawXIcon(Drivers::TftDisplay& tft, int cx, int cy, uint16_t color) {
    for (int i = 0; i < 15; i++) {
        tft.fillRect(cx - 7 + i, cy - 7 + i, 2, 2, color); // top-left to bottom-right
        tft.fillRect(cx + 7 - i, cy - 7 + i, 2, 2, color); // top-right to bottom-left
    }
}

// Helper: draw a lock icon using fillRect/drawRect only
static void drawLockIcon(Drivers::TftDisplay& tft, int cx, int cy, uint16_t color) {
    // Lock body
    tft.fillRect(cx - 8, cy - 2, 16, 12, color);
    // Lock shackle
    tft.drawRect(cx - 5, cy - 10, 10, 10, color);
    tft.drawRect(cx - 4, cy - 9,  8,  8, color);
    // Keyhole (small dark square)
    tft.fillRect(cx - 2, cy + 1, 4, 4, Theme::ColorBackground);
}

// Helper: draw WiFi signal bars using fillRect only
static void drawWifiIcon(Drivers::TftDisplay& tft, int cx, int cy, uint16_t color) {
    // 3 ascending bars
    tft.fillRect(cx - 8, cy - 2, 4, 6,  color);  // small bar
    tft.fillRect(cx - 2, cy - 6, 4, 10, color);  // medium bar
    tft.fillRect(cx + 4, cy - 10, 4, 14, color); // tall bar
    // Dot on top
    tft.fillRect(cx - 1, cy + 6, 2, 2, color);
}

// Helper: horizontal dotted separator line using fillRect
static void drawSeparator(Drivers::TftDisplay& tft, uint8_t y, uint16_t color) {
    for (int x = 20; x < tft.width() - 20; x += 3) {
        tft.fillRect(x, y, 1, 1, color);
    }
}

void ProvisioningScreen::renderScreen(ProvisioningState s) {
    auto& tft = Drivers::TftDisplay::getInstance();
    
    if (s == ProvisioningState::QR) { 
        drawQRScreen(); 
        return; 
    }

    // Clear screen
    tft.fillScreen(Theme::ColorBackground);
    
    switch (s) {
        case ProvisioningState::Boot:
            drawTitleBar("SMART BOWL", Theme::ColorAccent, Theme::ColorBackground);
            drawCentered("Welcome", 40, 2, Theme::ColorTextPrimary);
            drawSeparator(tft, 62, Theme::ColorTextSecondary);
            drawCentered("Initializing...", 72, 1, Theme::ColorAccent);
            drawCentered((String("v") + FW_VERSION).c_str(), 105, 1, Theme::ColorTextSecondary);
            break;

        case ProvisioningState::DeviceInfo:
            drawTitleBar("DEVICE INFO", Theme::ColorAccent, Theme::ColorBackground);
            drawWrapped("MAC Address:", _deviceMAC, 32);
            drawSeparator(tft, 62, Theme::ColorTextSecondary);
            drawWrapped("Firmware:", String("v") + FW_VERSION, 70);
            break;

        case ProvisioningState::Advertising:
            drawTitleBar("BLUETOOTH", 0x001F /*BLUE*/, Theme::ColorTextPrimary);
            drawCentered("Searching...", 38, 1, Theme::ColorTextPrimary);
            drawSeparator(tft, 52, Theme::ColorTextSecondary);
            drawCentered("ESP32-WiFi-Setup", 62, 1, Theme::ColorAccent);
            drawCentered("Scan QR to connect", 90, 1, Theme::ColorTextSecondary);
            break;

        case ProvisioningState::PhoneConnected:
            drawTitleBar("CONNECTED", Theme::ColorSuccess, Theme::ColorBackground);
            drawCheckIcon(tft, tft.width()/2, 50, Theme::ColorSuccess);
            drawSeparator(tft, 68, Theme::ColorTextSecondary);
            drawCentered("Phone linked!", 78, 1, Theme::ColorTextPrimary);
            drawCentered("Verifying PIN...", 96, 1, Theme::ColorAccent);
            break;

        case ProvisioningState::EnterPin:
            drawTitleBar("ENTER PIN", 0xFD20 /*ORANGE*/, Theme::ColorBackground);
            drawLockIcon(tft, tft.width()/2, 50, 0xFD20);
            drawSeparator(tft, 68, Theme::ColorTextSecondary);
            drawCentered("Waiting for PIN", 78, 1, Theme::ColorTextPrimary);
            drawCentered("from your app", 94, 1, Theme::ColorTextSecondary);
            break;

        case ProvisioningState::PinOk:
            drawTitleBar("PIN ACCEPTED", Theme::ColorSuccess, Theme::ColorBackground);
            drawCheckIcon(tft, tft.width()/2, 50, Theme::ColorSuccess);
            drawSeparator(tft, 68, Theme::ColorTextSecondary);
            drawCentered("Authenticated!", 78, 1, Theme::ColorSuccess);
            drawCentered("Send WiFi details", 96, 1, Theme::ColorTextPrimary);
            break;

        case ProvisioningState::PinFail:
            drawTitleBar("ACCESS DENIED", Theme::ColorError, Theme::ColorTextPrimary);
            drawXIcon(tft, tft.width()/2, 50, Theme::ColorError);
            drawSeparator(tft, 68, Theme::ColorTextSecondary);
            drawCentered("Wrong PIN!", 78, 1, Theme::ColorError);
            drawCentered("Please try again", 96, 1, Theme::ColorTextSecondary);
            break;

        case ProvisioningState::Locked:
            drawTitleBar("LOCKED", Theme::ColorError, Theme::ColorTextPrimary);
            drawLockIcon(tft, tft.width()/2, 50, Theme::ColorError);
            drawSeparator(tft, 68, Theme::ColorTextSecondary);
            drawCentered("Scan QR first!", 78, 1, Theme::ColorTextPrimary);
            drawCentered("PIN is required", 96, 1, Theme::ColorError);
            break;

        case ProvisioningState::Sending:
            drawTitleBar("CREDENTIALS", Theme::ColorAccent, Theme::ColorBackground);
            drawWrapped("Network:", _connectedSSID, 32);
            drawSeparator(tft, 62, Theme::ColorTextSecondary);
            drawCentered("Password received", 72, 1, Theme::ColorSuccess);
            drawCentered("Connecting...", 96, 1, Theme::ColorWarning);
            break;

        case ProvisioningState::WifiConnecting:
            drawTitleBar("CONNECTING", Theme::ColorWarning, Theme::ColorBackground);
            drawWifiIcon(tft, tft.width()/2, 48, Theme::ColorWarning);
            drawSeparator(tft, 66, Theme::ColorTextSecondary);
            drawWrapped("Network:", _connectedSSID, 72);
            drawCentered("Please wait...", 102, 1, Theme::ColorTextSecondary);
            break;

        case ProvisioningState::WifiConnected:
        case ProvisioningState::AutoConnected:
            drawTitleBar(s == ProvisioningState::AutoConnected ? "AUTO CONNECTED" : "WIFI CONNECTED", Theme::ColorSuccess, Theme::ColorBackground);
            drawWifiIcon(tft, tft.width()/2, 42, Theme::ColorSuccess);
            drawSeparator(tft, 58, Theme::ColorTextSecondary);
            drawCentered(_connectedSSID.c_str(), 66, 1, Theme::ColorTextPrimary);
            drawCentered("IP Address:", 84, 1, Theme::ColorTextSecondary);
            drawCentered(_connectedIP.c_str(), 98, 1, Theme::ColorAccent);
            break;

        case ProvisioningState::WifiFailed:
            drawTitleBar("CONN. FAILED", Theme::ColorError, Theme::ColorTextPrimary);
            drawXIcon(tft, tft.width()/2, 46, Theme::ColorError);
            drawSeparator(tft, 62, Theme::ColorTextSecondary);
            drawCentered(_connectedSSID.c_str(), 72, 1, Theme::ColorWarning);
            drawCentered("Check credentials", 96, 1, Theme::ColorTextSecondary);
            break;

        default: break;
    }
}

} // namespace Ui
} // namespace App
