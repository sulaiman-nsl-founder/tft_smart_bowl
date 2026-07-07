#ifndef APP_UI_PROVISIONING_SCREEN_H
#define APP_UI_PROVISIONING_SCREEN_H

#include "app/ui/Screen.h"
#include "app/ProvisioningStates.h"
#include <WString.h>

namespace App {
namespace Ui {

class ProvisioningScreen : public Screen {
public:
    static ProvisioningScreen& getInstance();

    void onEnter() override;
    void onExit() override;
    void onUpdate() override;
    bool onEvent(const Services::SystemEvent& event) override;

private:
    ProvisioningScreen() = default;
    ~ProvisioningScreen() = default;
    ProvisioningScreen(const ProvisioningScreen&) = delete;
    ProvisioningScreen& operator=(const ProvisioningScreen&) = delete;

    void renderScreen(ProvisioningState s);
    void drawQRScreen();
    void drawTitleBar(const char* title, uint16_t bgColor, uint16_t textColor);
    void drawCentered(const char* text, uint8_t y, uint8_t size, uint16_t color = 0xFFFF); // 0xFFFF = ST77XX_WHITE
    void drawWrapped(const char* label, const String& value, uint8_t y);

    ProvisioningState _currentState = ProvisioningState::None;
    bool _needsRedraw = false;
    
    String _deviceMAC = "";
    String _deviceInfo = "";
    String _connectedSSID = "";
    String _connectedIP = "";
};

} // namespace Ui
} // namespace App

#endif // APP_UI_PROVISIONING_SCREEN_H
