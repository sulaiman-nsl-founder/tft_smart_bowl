#ifndef APP_UI_OTA_SCREEN_H
#define APP_UI_OTA_SCREEN_H

#include "app/ui/Screen.h"

namespace App {
namespace Ui {

/**
 * @brief OTA update progress screen.
 * 
 * Shows a simulated (for now) or real progress bar for firmware updates.
 */
class OtaScreen : public Screen {
public:
    static OtaScreen& getInstance();

    void onEnter() override;
    void onExit() override;
    void onUpdate() override;
    bool onEvent(const Services::SystemEvent& event) override;

private:
    OtaScreen() = default;
    ~OtaScreen() = default;
    OtaScreen(const OtaScreen&) = delete;
    OtaScreen& operator=(const OtaScreen&) = delete;

    enum class State : uint8_t {
        Checking,
        Downloading,
        Installing,
        Done,
        Error
    };

    void drawChecking();
    void drawProgress(const char* label);
    void drawDone();
    void drawError();
    void drawTitleBar(const char* title, uint16_t bgColor, uint16_t textColor);
    void drawCentered(const char* text, uint8_t y, uint8_t size, uint16_t color);

    State _state = State::Checking;
    bool _needsRedraw = true;
    uint8_t _progressPct = 0;
    uint32_t _lastUpdateTimer = 0;
};

} // namespace Ui
} // namespace App

#endif // APP_UI_OTA_SCREEN_H
