#ifndef APP_UI_SCREEN_H
#define APP_UI_SCREEN_H

#include "services/EventBus.h"

namespace App {
namespace Ui {

/**
 * @brief Base interface for a UI screen.
 */
class Screen {
public:
    virtual ~Screen() = default;

    /**
     * @brief Called when the screen becomes active.
     * Perfect for drawing the static background and layout.
     */
    virtual void onEnter() = 0;

    /**
     * @brief Called when the screen is deactivated.
     */
    virtual void onExit() = 0;

    /**
     * @brief Called periodically (e.g., 10Hz) to allow the screen to update animations or dynamic text.
     */
    virtual void onUpdate() = 0;

    /**
     * @brief Handle a system event (button press, weight update).
     * @param event The event to handle.
     * @return true if the event was consumed, false otherwise.
     */
    virtual bool onEvent(const Services::SystemEvent& event) = 0;
};

} // namespace Ui
} // namespace App

#endif // APP_UI_SCREEN_H
