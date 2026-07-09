#ifndef APP_UI_SETTINGS_SCREEN_H
#define APP_UI_SETTINGS_SCREEN_H

#include "app/ui/Screen.h"
#include <WString.h>

namespace App {
namespace Ui {

/**
 * @brief Main settings menu.
 * 
 * Accessed by long-pressing Button 1 + Button 3.
 * Provides a menu to navigate to Calibration, OTA update, etc.
 */
class SettingsScreen : public Screen {
public:
    static SettingsScreen& getInstance();

    void onEnter() override;
    void onExit() override;
    void onUpdate() override;
    bool onEvent(const Services::SystemEvent& event) override;

private:
    SettingsScreen() = default;
    ~SettingsScreen() = default;
    SettingsScreen(const SettingsScreen&) = delete;
    SettingsScreen& operator=(const SettingsScreen&) = delete;

    void drawMenu(bool full);
    void drawTitleBar(const char* title, uint16_t bgColor, uint16_t textColor);

    uint8_t _menuIndex = 0;
    static constexpr uint8_t MENU_ITEMS = 3;
    bool _fullRedraw = true;
    bool _menuRedraw = true;
};

} // namespace Ui
} // namespace App

#endif // APP_UI_SETTINGS_SCREEN_H
