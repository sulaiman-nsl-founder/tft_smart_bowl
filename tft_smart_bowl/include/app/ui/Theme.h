#ifndef APP_UI_THEME_H
#define APP_UI_THEME_H

#include "drivers/TftDisplay.h"

namespace App {
namespace Ui {

/**
 * @brief Common visual constants and colors for the Smart Bowl UI.
 */
struct Theme {
    static constexpr uint16_t ColorBackground = Drivers::TftDisplay::COLOR_BLACK;
    static constexpr uint16_t ColorTextPrimary  = Drivers::TftDisplay::COLOR_WHITE;
    static constexpr uint16_t ColorTextSecondary = 0x8410; // Gray
    static constexpr uint16_t ColorAccent       = 0x07FF; // Cyan
    static constexpr uint16_t ColorSuccess      = Drivers::TftDisplay::COLOR_GREEN;
    static constexpr uint16_t ColorWarning      = Drivers::TftDisplay::COLOR_YELLOW;
    static constexpr uint16_t ColorError        = Drivers::TftDisplay::COLOR_RED;
};

} // namespace Ui
} // namespace App

#endif // APP_UI_THEME_H
