#pragma once
#include <Arduino.h>

namespace Services {

class CloudSync {
public:
    static CloudSync& getInstance();
    void begin();
    
private:
    CloudSync() = default;
    
    static void taskRoutine(void* pvParameters);
    void run();

    TaskHandle_t _taskHandle = nullptr;
    uint32_t _lastTelemetryMs = 0;
    uint32_t _seq = 0;
    bool _wasConnected = false;
};

} // namespace Services
