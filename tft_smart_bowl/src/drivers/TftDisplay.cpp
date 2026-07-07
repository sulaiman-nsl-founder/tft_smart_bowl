#include "drivers/TftDisplay.h"
#include "drivers/SpiBus.h"
#include "board/Pins.h"
#include "services/Logger.h"
#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

namespace Drivers {

// Static display instance (same pattern as reference code)
static Adafruit_ST7735* s_tft = nullptr;

TftDisplay& TftDisplay::getInstance() {
    static TftDisplay instance;
    return instance;
}

Core::Result<void> TftDisplay::begin() {
    if (_initialized) return Core::Result<void>();

    auto& spiBus = SpiBus::getInstance();
    if (!spiBus.isInitialized()) {
        return Core::Result<void>(Core::Error(
            Core::ErrorCategory::SPI, Core::ErrorCode::InvalidState, "SPI bus not initialized"));
    }

    // Create the display object using the FSPI instance
    // Reference: Adafruit_ST7735 tft = Adafruit_ST7735(&TFTSPI, TFT_CS, TFT_DC, TFT_RST);
    SPIClass* spi = static_cast<SPIClass*>(spiBus.spiInstance());
    s_tft = new Adafruit_ST7735(spi, 
                                 Board::Pins::TFT_CS, 
                                 Board::Pins::TFT_DC, 
                                 Board::Pins::TFT_RST);

    if (s_tft == nullptr) {
        return Core::Result<void>(Core::Error(
            Core::ErrorCategory::System, Core::ErrorCode::NoMemory, "Failed to allocate TFT display"));
    }

    // Initialize the ST7735 — use initR(INITR_BLACKTAB) for most ST7735 variants
    s_tft->initR(INITR_BLACKTAB);
    s_tft->setRotation(1);  // Landscape
    s_tft->fillScreen(ST77XX_BLACK);

    _tft = s_tft;
    _initialized = true;
    LOG_INFO("TFT", 800, "ST7735 display initialized (%dx%d)", s_tft->width(), s_tft->height());
    return Core::Result<void>();
}

void TftDisplay::clear(uint16_t color) {
    if (!_initialized) return;
    s_tft->fillScreen(color);
}

void TftDisplay::fillScreen(uint16_t color) {
    if (!_initialized) return;
    s_tft->fillScreen(color);
}

void TftDisplay::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (!_initialized) return;
    s_tft->fillRect(x, y, w, h, color);
}

void TftDisplay::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (!_initialized) return;
    s_tft->drawRect(x, y, w, h, color);
}

void TftDisplay::setCursor(int16_t x, int16_t y) {
    if (!_initialized) return;
    s_tft->setCursor(x, y);
}

void TftDisplay::setTextColor(uint16_t color) {
    if (!_initialized) return;
    s_tft->setTextColor(color);
}

void TftDisplay::setTextColor(uint16_t color, uint16_t bg) {
    if (!_initialized) return;
    s_tft->setTextColor(color, bg);
}

void TftDisplay::setTextSize(uint8_t size) {
    if (!_initialized) return;
    s_tft->setTextSize(size);
}

void TftDisplay::print(const char* text) {
    if (!_initialized) return;
    s_tft->print(text);
}

void TftDisplay::println(const char* text) {
    if (!_initialized) return;
    s_tft->println(text);
}

void TftDisplay::showTestPattern() {
    if (!_initialized) return;

    int16_t w = s_tft->width();
    int16_t h = s_tft->height();
    int16_t barWidth = w / 8;

    // Draw 8 color bars
    uint16_t colors[] = {
        COLOR_WHITE, COLOR_YELLOW, COLOR_CYAN, COLOR_GREEN,
        COLOR_MAGENTA, COLOR_RED, COLOR_BLUE, COLOR_BLACK
    };

    for (int i = 0; i < 8; i++) {
        s_tft->fillRect(i * barWidth, 0, barWidth, h, colors[i]);
    }

    // Orientation label
    s_tft->setTextSize(1);
    s_tft->setTextColor(COLOR_BLACK, COLOR_WHITE);
    s_tft->setCursor(4, 4);
    s_tft->print("SMART-BOWL TFT TEST");
    
    s_tft->setCursor(4, h - 12);
    char buf[32];
    snprintf(buf, sizeof(buf), "%dx%d R=%d", w, h, s_tft->getRotation());
    s_tft->print(buf);

    LOG_INFO("TFT", 810, "Test pattern displayed");
}

void TftDisplay::setRotation(uint8_t r) {
    if (!_initialized) return;
    s_tft->setRotation(r);
}

int16_t TftDisplay::width() const {
    if (!_initialized) return 0;
    return s_tft->width();
}

int16_t TftDisplay::height() const {
    if (!_initialized) return 0;
    return s_tft->height();
}

bool TftDisplay::isInitialized() const {
    return _initialized;
}

} // namespace Drivers
