#ifndef APP_UI_DASHBOARD_SCREEN_H
#define APP_UI_DASHBOARD_SCREEN_H

#include "app/ui/Screen.h"

namespace App {
namespace Ui {

class DashboardScreen : public Screen {
public:
    static DashboardScreen& getInstance();

    void onEnter() override;
    void onExit() override;
    void onUpdate() override;
    bool onEvent(const Services::SystemEvent& event) override;

private:
    DashboardScreen() = default;

    float _lastWeight = 0.0f;
    bool _lastStable = false;
    bool _needsRedraw = true;
};

} // namespace Ui
} // namespace App

#endif // APP_UI_DASHBOARD_SCREEN_H
