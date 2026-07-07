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
    
    // Dynamically scale the QR code to fill the screen height beautifully
    uint8_t scale = 120 / size; 
    if (scale < 1) scale = 1;
    uint8_t margin = (128 - (size * scale)) / 2; // Vertically center
    uint8_t qrW = size * scale;

    // Draw a solid white box behind the QR code
    tft.fillRect(0, 0, (margin * 2) + qrW, tft.height(), Drivers::TftDisplay::COLOR_WHITE);

    // Draw black modules
    for (uint8_t y = 0; y < size; y++) {
        for (uint8_t x = 0; x < size; x++) {
            if (esp_qrcode_get_module(qrcode, x, y)) {
                tft.fillRect(margin + (x * scale), margin + (y * scale), scale, scale, Drivers::TftDisplay::COLOR_BLACK);
            }
        }
    }

    // Draw instructions on the right side
    uint8_t tx = (margin * 2) + qrW + 5;
    tft.setTextSize(1);
    tft.setTextColor(Drivers::TftDisplay::COLOR_WHITE);
    tft.setCursor(tx, 20); tft.print("SCAN");
    tft.setCursor(tx, 35); tft.print("QR TO");
    tft.setCursor(tx, 50); tft.print("SETUP");
    tft.setCursor(tx, 70); tft.print("PIN:");
    
    tft.setTextSize(2);
    tft.setTextColor(Drivers::TftDisplay::COLOR_YELLOW);
    tft.setCursor(tx, 85); tft.print(POP_PIN);
    tft.setTextColor(Drivers::TftDisplay::COLOR_WHITE); // reset
}

void ProvisioningScreen::drawQRScreen() {
    auto& tft = Drivers::TftDisplay::getInstance();
    tft.fillScreen(Drivers::TftDisplay::COLOR_BLACK);
    
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
    tft.fillRect(0, 0, tft.width(), 18, bgColor);
    tft.setTextColor(textColor, bgColor);
    tft.setTextSize(1);
    
    // Calculate center (using simple approximation since TftDisplay might not have getTextBounds)
    int len = strlen(title);
    int charWidth = 6; // text size 1 is 5x7 + 1px spacing
    int w = len * charWidth;
    int x = (tft.width() - w) / 2;
    if (x < 0) x = 0;
    
    tft.setCursor(x, 5);
    tft.print(title);
    tft.setTextColor(Drivers::TftDisplay::COLOR_WHITE, Drivers::TftDisplay::COLOR_BLACK);
}

void ProvisioningScreen::drawCentered(const char* text, uint8_t y, uint8_t size, uint16_t color) {
    auto& tft = Drivers::TftDisplay::getInstance();
    tft.setTextSize(size);
    tft.setTextColor(color, Drivers::TftDisplay::COLOR_BLACK);
    
    int len = strlen(text);
    int charWidth = 6 * size;
    int w = len * charWidth;
    int x = (tft.width() - w) / 2;
    if (x < 0) x = 0;
    
    tft.setCursor(x, y);
    tft.print(text);
    tft.setTextColor(Drivers::TftDisplay::COLOR_WHITE, Drivers::TftDisplay::COLOR_BLACK);
}

void ProvisioningScreen::drawWrapped(const char* label, const String& value, uint8_t y) {
    auto& tft = Drivers::TftDisplay::getInstance();
    tft.setTextSize(1);
    tft.setCursor(5, y);
    tft.print(label);
    tft.setCursor(5, y + 15);
    tft.print(value.substring(0, 25).c_str());
}

void ProvisioningScreen::renderScreen(ProvisioningState s) {
    auto& tft = Drivers::TftDisplay::getInstance();
    
    if (s == ProvisioningState::QR) { 
        drawQRScreen(); 
        return; 
    }

    // Clear screen immediately for all other views
    tft.fillScreen(Drivers::TftDisplay::COLOR_BLACK);
    
    switch (s) {
        case ProvisioningState::Boot:
            drawCentered("ESP32", 30, 2, Drivers::TftDisplay::COLOR_CYAN);
            drawCentered("WiFi Provisioner", 60, 1);
            drawCentered((String("FW: ") + FW_VERSION).c_str(), 80, 1);
            drawCentered(_deviceMAC.c_str(), 100, 1);
            break;

        case ProvisioningState::DeviceInfo:
            drawTitleBar("DEVICE INFO", Drivers::TftDisplay::COLOR_WHITE, Drivers::TftDisplay::COLOR_BLACK);
            tft.setCursor(5, 40); tft.print("MAC:");
            tft.setCursor(5, 55); tft.print(_deviceMAC.c_str());
            tft.setCursor(5, 80); tft.print("Firmware:");
            tft.setCursor(5, 95); tft.print((String("v") + FW_VERSION).c_str());
            break;

        case ProvisioningState::Advertising:
            drawTitleBar("BLUETOOTH", Drivers::TftDisplay::COLOR_BLUE, Drivers::TftDisplay::COLOR_WHITE);
            drawCentered("Advertising...", 50, 1);
            drawCentered("ESP32-WiFi-Setup", 75, 1, Drivers::TftDisplay::COLOR_YELLOW);
            drawCentered("Scan QR to connect", 100, 1);
            break;

        case ProvisioningState::PhoneConnected:
            drawTitleBar("BLE CONNECTED", Drivers::TftDisplay::COLOR_GREEN, Drivers::TftDisplay::COLOR_BLACK);
            drawCentered("Phone connected!", 50, 1);
            drawCentered("Verifying PIN...", 80, 1, Drivers::TftDisplay::COLOR_CYAN);
            break;

        case ProvisioningState::EnterPin:
            drawTitleBar("SECURITY", 0xFD20 /*ORANGE*/, Drivers::TftDisplay::COLOR_BLACK);
            drawCentered("Waiting for PIN", 50, 1);
            drawCentered("Scan QR code", 75, 1);
            drawCentered("to authenticate", 90, 1);
            break;

        case ProvisioningState::PinOk:
            drawTitleBar("PIN ACCEPTED", Drivers::TftDisplay::COLOR_GREEN, Drivers::TftDisplay::COLOR_BLACK);
            tft.drawRect(50, 35, 60, 45, Drivers::TftDisplay::COLOR_GREEN);
            drawCentered("OK", 50, 2, Drivers::TftDisplay::COLOR_GREEN);
            drawCentered("Send WiFi details", 100, 1);
            break;

        case ProvisioningState::PinFail:
            drawTitleBar("ACCESS DENIED", Drivers::TftDisplay::COLOR_RED, Drivers::TftDisplay::COLOR_WHITE);
            tft.drawRect(50, 35, 60, 45, Drivers::TftDisplay::COLOR_RED);
            drawCentered("X", 50, 2, Drivers::TftDisplay::COLOR_RED);
            drawCentered("Wrong PIN-rescan", 100, 1);
            break;

        case ProvisioningState::Locked:
            drawTitleBar("LOCKED", Drivers::TftDisplay::COLOR_RED, Drivers::TftDisplay::COLOR_WHITE);
            drawCentered("Scan QR first!", 50, 1);
            drawCentered("PIN required", 80, 1, Drivers::TftDisplay::COLOR_RED);
            break;

        case ProvisioningState::Sending:
            drawTitleBar("CREDENTIALS RX", Drivers::TftDisplay::COLOR_WHITE, Drivers::TftDisplay::COLOR_BLACK);
            drawWrapped("SSID:", _connectedSSID, 40);
            drawCentered("Password received", 80, 1);
            drawCentered("Connecting...", 100, 1, Drivers::TftDisplay::COLOR_YELLOW);
            break;

        case ProvisioningState::WifiConnecting:
            drawTitleBar("CONNECTING...", Drivers::TftDisplay::COLOR_YELLOW, Drivers::TftDisplay::COLOR_BLACK);
            drawWrapped("SSID:", _connectedSSID, 40);
            drawCentered("Please wait", 90, 1);
            break;

        case ProvisioningState::WifiConnected:
        case ProvisioningState::AutoConnected:
            drawTitleBar(s == ProvisioningState::AutoConnected ? "AUTO CONNECTED" : "WIFI CONNECTED", Drivers::TftDisplay::COLOR_GREEN, Drivers::TftDisplay::COLOR_BLACK);
            drawCentered(_connectedSSID.c_str(), 45, 1);
            tft.setCursor(5, 75); tft.print("IP:");
            drawCentered(_connectedIP.c_str(), 95, 1, Drivers::TftDisplay::COLOR_CYAN);
            break;

        case ProvisioningState::WifiFailed:
            drawTitleBar("CONN. FAILED", Drivers::TftDisplay::COLOR_RED, Drivers::TftDisplay::COLOR_WHITE);
            drawCentered("Cannot connect to:", 45, 1);
            drawCentered(_connectedSSID.c_str(), 70, 1, Drivers::TftDisplay::COLOR_YELLOW);
            drawCentered("Check credentials", 100, 1);
            break;

        default: break;
    }
}

} // namespace Ui
} // namespace App
