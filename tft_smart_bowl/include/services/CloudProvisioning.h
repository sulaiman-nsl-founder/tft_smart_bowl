#pragma once
#include <Arduino.h>

namespace Services {

class CloudProvisioning {
public:
    static CloudProvisioning& getInstance();
    void begin();
    
private:
    CloudProvisioning() = default;
    
    static void taskRoutine(void* pvParameters);
    void run();
    
    bool doProvisioning();
    bool parseResponse(const String& response);

    TaskHandle_t _taskHandle = nullptr;
    bool _provisioned = false;
};

} // namespace Services
