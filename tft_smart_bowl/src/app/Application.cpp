#include "app/Application.h"
#include <Arduino.h>
#include "core/Result.h"
#include "services/Logger.h"
#include "services/Supervisor.h"
#include "drivers/GpioSafeState.h"
#include "drivers/Buttons.h"
#include "drivers/Leds.h"
#include "platform/ResetInfo.h"
#include "app/BoardSelfTest.h"
#include "services/WeightService.h"
#include "services/EventBus.h"
#include "app/ui/UiManager.h"
#include "app/ui/DashboardScreen.h"
#include "app/ui/ProvisioningScreen.h"
#include "app/BowlStateMachine.h"
#include "app/FeedingSession.h"
#include "app/FoodMenu.h"
#include "drivers/TftDisplay.h"
#include "drivers/SdCard.h"
#include "services/OfflineQueue.h"
#include "services/ProvisioningService.h"
#include "platform/BuildInfo.h"

// Simple embedded unit tests to verify core classes on target hardware
void run_embedded_unit_tests() {
    using namespace Core;
    using namespace Services;
    
    Logger::getInstance().log(LogLevel::Info, "TEST", 100, "--- Starting Embedded Unit Tests ---");
    
    // Test Result success
    Result<int> res(42);
    if (res.isOk() && res.value() == 42) {
        Logger::getInstance().log(LogLevel::Info, "TEST", 101, "test_result_success: PASS");
    } else {
        Logger::getInstance().log(LogLevel::Error, "TEST", 101, "test_result_success: FAIL");
    }
    
    // Test Result error
    Error err(ErrorCategory::Sensor, ErrorCode::DeviceNotFound, "Sensor not found");
    Result<int> res_err(err);
    if (res_err.isError() && res_err.error().code == ErrorCode::DeviceNotFound) {
        Logger::getInstance().log(LogLevel::Info, "TEST", 102, "test_result_error: PASS");
    } else {
        Logger::getInstance().log(LogLevel::Error, "TEST", 102, "test_result_error: FAIL");
    }
    
    // Test Logger Secret Redaction
    Logger::getInstance().registerSecret("supersecret123");
    char buffer[128] = "This is a supersecret123 message";
    Logger::getInstance().redact(buffer);
    if (strcmp(buffer, "This is a *************** message") == 0) {
         Logger::getInstance().log(LogLevel::Info, "TEST", 103, "test_logger_redaction: PASS");
    } else {
         Logger::getInstance().log(LogLevel::Error, "TEST", 103, "test_logger_redaction: FAIL");
    }
    Logger::getInstance().clearSecrets();
    
    Logger::getInstance().log(LogLevel::Info, "TEST", 104, "--- Embedded Unit Tests Complete ---");
}

Application& Application::getInstance() {
    static Application instance;
    return instance;
}

void Application::setup() {
    // 1. Safe GPIO states first — prevent hardware glitches
    Board::GpioSafeState::initialize();
    
    Serial.begin(115200);
    delay(100);  // Allow serial to stabilize
    
    // 2. Capture reset reason ASAP
    Platform::ResetInfo::capture();
    
    // 3. Increment boot failure counter (will be cleared on success)
    uint32_t failCount = Platform::ResetInfo::incrementBootFailCount();
    
    // 4. Initialize logging
    Services::Logger::getInstance().setMinLevel(Services::LogLevel::Debug);
    
    // 5. Print boot banner
    LOG_INFO("BOOT", 1, "=== SMART-BOWL Firmware ===");
    LOG_INFO("BOOT", 2, "Version: %s", FW_VERSION);
    LOG_INFO("BOOT", 3, "Build: %s", BUILD_DATE);
    LOG_INFO("BOOT", 4, "Reset reason: %s (raw: %u)", 
             Platform::ResetInfo::reasonString(), 
             Platform::ResetInfo::rawCode());
    LOG_INFO("BOOT", 5, "Boot fail count: %u / %u", 
             failCount, Platform::ResetInfo::SAFE_MODE_THRESHOLD);
    
    // 6. Check for safe mode
    if (Platform::ResetInfo::shouldEnterSafeMode()) {
        LOG_CRIT("BOOT", 6, "*** SAFE MODE ACTIVATED *** — Too many consecutive boot failures!");
        // In safe mode: skip peripheral init, only run diagnostics
        // For now, just log it. Full safe mode will be implemented later.
        Platform::ResetInfo::markBootSuccess(); // Reset counter so we can try again
        return;
    }
    
    // 7. Initialize watchdog supervisor (5 second timeout)
    Services::Supervisor::getInstance().begin(5000);
    Services::Supervisor::getInstance().registerCurrentTask();
    
    // 8. Run board self-test (I2C, MCP, Buttons, LEDs, SPI, TFT)
    App::BoardSelfTest::run();
    
    // Initialize SD Card right after self-test but before services that need it
    Drivers::SdCard::getInstance().begin();
    
    // 9. Start background services
    Services::EventBus::getInstance().begin(32);
    Services::WeightService::getInstance().begin();
    Services::OfflineQueue::getInstance().begin();
    App::FoodMenu::getInstance().begin();
    App::BowlStateMachine::getInstance().begin();
    App::FeedingSession::getInstance().begin();
    Services::ProvisioningService::getInstance().begin();
    
    // 10. Start UI Manager and set initial screen
    App::Ui::UiManager::getInstance().begin();
    App::Ui::UiManager::getInstance().setScreen(&App::Ui::ProvisioningScreen::getInstance());
    
    // 11. Run embedded unit tests
    run_embedded_unit_tests();
    
    // 12. Mark boot as successful — reset the failure counter
    Platform::ResetInfo::markBootSuccess();
    LOG_INFO("BOOT", 10, "Boot complete. System ready.");

    // 13. Setup BOOT button (GPIO 0) for factory reset
    pinMode(0, INPUT_PULLUP);
}

void Application::loop() {
    // Feed the watchdog every loop iteration
    Services::Supervisor::getInstance().feed();
    
    // Update button states (polls MCP23017)
    Drivers::Buttons::getInstance().update();
    
    // Update LED patterns (handles blink timing)
    Drivers::Leds::getInstance().update();
    
    // Map button events to EventBus
    for (uint8_t i = 0; i < 3; i++) {
        auto evt = Drivers::Buttons::getInstance().getEvent(static_cast<Drivers::ButtonId>(i));
        if (evt != Drivers::ButtonEvent::None) {
            Services::SystemEvent sysEvt;
            sysEvt.id = Services::EventId::ButtonPress;
            sysEvt.timestampMs = millis();
            sysEvt.payload.button.buttonId = i;
            sysEvt.payload.button.eventType = (evt == Drivers::ButtonEvent::Press) ? 1 :
                                              (evt == Drivers::ButtonEvent::Release) ? 2 :
                                              (evt == Drivers::ButtonEvent::LongPress) ? 3 : 0;
            Services::EventBus::getInstance().publish(sysEvt);
            
            // Still toggle LEDs for physical feedback
            if (evt == Drivers::ButtonEvent::Press) {
                auto& leds = Drivers::Leds::getInstance();
                auto ledId = static_cast<Drivers::LedId>(i);
                if (leds.isOn(ledId)) {
                    leds.off(ledId);
                } else {
                    leds.on(ledId);
                }
            }
            
            // Handle Provisioning reset on button 1 long press
            if (i == 1 && evt == Drivers::ButtonEvent::LongPress) {
                Services::ProvisioningService::getInstance().resetCredentials();
                App::Ui::UiManager::getInstance().setScreen(&App::Ui::ProvisioningScreen::getInstance());
            }
        }
    }

    // Check BOOT button (GPIO 0) for Factory Reset / WiFi Reset
    static uint32_t bootBtnPressTime = 0;
    static bool bootBtnWasPressed = false;
    static bool bootBtnLongPressFired = false;
    
    bool bootBtnIsPressed = (digitalRead(0) == LOW);
    uint32_t now = millis();
    
    if (bootBtnIsPressed && !bootBtnWasPressed) {
        bootBtnPressTime = now;
        bootBtnWasPressed = true;
        bootBtnLongPressFired = false;
    } else if (!bootBtnIsPressed && bootBtnWasPressed) {
        bootBtnWasPressed = false;
    }
    
    if (bootBtnIsPressed && !bootBtnLongPressFired && (now - bootBtnPressTime >= 2000)) { // 2 seconds for long press
        bootBtnLongPressFired = true;
        LOG_WARN("APP", 200, "Factory reset triggered via BOOT button");
        Services::ProvisioningService::getInstance().resetCredentials();
        App::Ui::UiManager::getInstance().setScreen(&App::Ui::ProvisioningScreen::getInstance());
    }
}

