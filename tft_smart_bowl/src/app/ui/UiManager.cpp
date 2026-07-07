#include "app/ui/UiManager.h"
#include "services/EventBus.h"
#include "services/Logger.h"
#include "services/Supervisor.h"

namespace App {
namespace Ui {

UiManager& UiManager::getInstance() {
    static UiManager instance;
    return instance;
}

void UiManager::begin() {
    if (_taskHandle != nullptr) return;

    Services::EventBus::getInstance().subscribe([](const Services::SystemEvent& event) {
        UiManager::getInstance().handleEvent(event);
    });

    xTaskCreatePinnedToCore(
        taskMain,
        "UiTask",
        4096,
        this,
        4, // Priority (below WeightTask)
        &_taskHandle,
        1  // App core
    );
}

void UiManager::setScreen(Screen* newScreen) {
    if (_currentScreen == newScreen) return;
    
    if (_currentScreen != nullptr) {
        _currentScreen->onExit();
    }
    
    _currentScreen = newScreen;
    
    if (_currentScreen != nullptr) {
        _currentScreen->onEnter();
    }
}

void UiManager::handleEvent(const Services::SystemEvent& event) {
    if (_currentScreen != nullptr) {
        _currentScreen->onEvent(event);
    }
}

void UiManager::taskMain(void* pvParameters) {
    UiManager* manager = static_cast<UiManager*>(pvParameters);
    Services::Supervisor::getInstance().registerCurrentTask();
    manager->run();
}

void UiManager::run() {
    LOG_INFO("UI", 800, "UiManager task started");
    
    // We aim to update the UI at 20Hz (every 50ms)
    const TickType_t xFrequency = pdMS_TO_TICKS(50);
    TickType_t xLastWakeTime = xTaskGetTickCount();

    while (true) {
        Services::Supervisor::getInstance().feed();
        
        // 1. Allow current screen to run animations/updates
        if (_currentScreen != nullptr) {
            _currentScreen->onUpdate();
        }
        
        // 2. Sleep until next frame
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

} // namespace Ui
} // namespace App
