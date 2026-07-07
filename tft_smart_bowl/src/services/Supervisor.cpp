#include "services/Supervisor.h"
#include "services/Logger.h"
#include <Arduino.h>
#include "esp_task_wdt.h"

namespace Services {

Supervisor& Supervisor::getInstance() {
    static Supervisor instance;
    return instance;
}

void Supervisor::begin(uint32_t timeoutMs) {
    _timeoutMs = timeoutMs;

    // Use the legacy API compatible with this Arduino-ESP32 core version
    // esp_task_wdt_init(timeout_seconds, panic_on_timeout)
    uint32_t timeoutSec = _timeoutMs / 1000;
    if (timeoutSec < 1) timeoutSec = 1;
    
    esp_err_t err = esp_task_wdt_init(timeoutSec, true); // true = panic on timeout
    
    if (err == ESP_OK || err == ESP_ERR_INVALID_STATE) {
        // ESP_ERR_INVALID_STATE means already initialized, which is fine
        _active = true;
        LOG_INFO("WDT", 200, "Watchdog initialized: %u sec timeout", timeoutSec);
    } else {
        LOG_ERROR("WDT", 201, "Watchdog init failed: %d", err);
    }
}

void Supervisor::registerCurrentTask() {
    if (!_active) return;
    
    esp_err_t err = esp_task_wdt_add(NULL); // NULL = current task
    if (err == ESP_OK) {
        LOG_INFO("WDT", 202, "Task registered with watchdog");
    } else if (err == ESP_ERR_INVALID_STATE) {
        LOG_WARN("WDT", 203, "Task already registered with watchdog");
    } else {
        LOG_ERROR("WDT", 204, "Failed to register task: %d", err);
    }
}

void Supervisor::unregisterCurrentTask() {
    if (!_active) return;
    
    esp_err_t err = esp_task_wdt_delete(NULL);
    if (err == ESP_OK) {
        LOG_INFO("WDT", 205, "Task unregistered from watchdog");
    } else {
        LOG_ERROR("WDT", 206, "Failed to unregister task: %d", err);
    }
}

void Supervisor::feed() {
    if (!_active) return;
    esp_task_wdt_reset();
}

bool Supervisor::isActive() const {
    return _active;
}

} // namespace Services
