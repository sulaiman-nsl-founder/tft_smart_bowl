#ifndef APP_UI_MANAGER_H
#define APP_UI_MANAGER_H

#include "app/ui/Screen.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace App {
namespace Ui {

/**
 * @brief Manages the active UI screen and distributes events.
 * 
 * Runs a FreeRTOS task to periodically poll the EventBus and update the screen.
 */
class UiManager {
public:
    static UiManager& getInstance();

    /**
     * @brief Start the UI manager task.
     */
    void begin();

    /**
     * @brief Switch to a new screen.
     * @param newScreen Pointer to the new screen. The caller must ensure the 
     *        screen object outlives its usage (typically they are singletons).
     */
    void setScreen(Screen* newScreen);

    /**
     * @brief Handle an event from the EventBus.
     */
    void handleEvent(const Services::SystemEvent& event);

private:
    UiManager() = default;
    ~UiManager() = default;
    UiManager(const UiManager&) = delete;
    UiManager& operator=(const UiManager&) = delete;

    static void taskMain(void* pvParameters);
    void run();

    Screen* _currentScreen = nullptr;
    TaskHandle_t _taskHandle = nullptr;
};

} // namespace Ui
} // namespace App

#endif // APP_UI_MANAGER_H
