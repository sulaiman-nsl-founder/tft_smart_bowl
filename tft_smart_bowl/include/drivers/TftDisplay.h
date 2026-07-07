#ifndef DRIVERS_TFT_DISPLAY_H
#define DRIVERS_TFT_DISPLAY_H

#include <stdint.h>
#include "core/Result.h"
#include "core/Errors.h"

namespace Drivers {

/**
 * @brief ST7735 TFT display driver.
 *
 * Based on reference code BLEWiFiProvisioner.ino:
 *   Adafruit_ST7735 tft = Adafruit_ST7735(&TFTSPI, TFT_CS, TFT_DC, TFT_RST);
 *
 * Provides initialization, screen clearing, text, rectangles, and
 * a color bar test pattern.
 */
class TftDisplay {
public:
    static TftDisplay& getInstance();

    /**
     * @brief Initialize the TFT display.
     * Must be called after SpiBus::begin().
     * @return Success or error.
     */
    Core::Result<void> begin();

    /** @brief Clear the screen to a single color. */
    void clear(uint16_t color = 0x0000);

    /** @brief Fill the entire screen with a color. */
    void fillScreen(uint16_t color);

    /** @brief Draw a filled rectangle. */
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

    /** @brief Draw a rectangle outline. */
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

    /** @brief Set the text cursor position. */
    void setCursor(int16_t x, int16_t y);

    /** @brief Set the text color. */
    void setTextColor(uint16_t color);

    /** @brief Set the text color with background. */
    void setTextColor(uint16_t color, uint16_t bg);

    /** @brief Set the text size multiplier. */
    void setTextSize(uint8_t size);

    /** @brief Print text at current cursor position. */
    void print(const char* text);

    /** @brief Print text followed by newline. */
    void println(const char* text);

    /** @brief Display a color bar test pattern with orientation labels. */
    void showTestPattern();

    /** @brief Set display rotation (0-3). */
    void setRotation(uint8_t r);

    /** @brief Get screen width in pixels. */
    int16_t width() const;

    /** @brief Get screen height in pixels. */
    int16_t height() const;

    /** @brief Check if display is initialized. */
    bool isInitialized() const;

    // Common 16-bit (RGB565) color constants
    static constexpr uint16_t COLOR_BLACK   = 0x0000;
    static constexpr uint16_t COLOR_WHITE   = 0xFFFF;
    static constexpr uint16_t COLOR_RED     = 0xF800;
    static constexpr uint16_t COLOR_GREEN   = 0x07E0;
    static constexpr uint16_t COLOR_BLUE    = 0x001F;
    static constexpr uint16_t COLOR_YELLOW  = 0xFFE0;
    static constexpr uint16_t COLOR_CYAN    = 0x07FF;
    static constexpr uint16_t COLOR_MAGENTA = 0xF81F;

private:
    TftDisplay() = default;
    ~TftDisplay() = default;
    TftDisplay(const TftDisplay&) = delete;
    TftDisplay& operator=(const TftDisplay&) = delete;

    bool _initialized = false;
    void* _tft = nullptr;  // Adafruit_ST7735 pointer
};

} // namespace Drivers

#endif // DRIVERS_TFT_DISPLAY_H
