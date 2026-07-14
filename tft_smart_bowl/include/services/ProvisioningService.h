#ifndef SERVICES_PROVISIONING_SERVICE_H
#define SERVICES_PROVISIONING_SERVICE_H

#include <stdint.h>
#include <WString.h>
#include "app/ProvisioningStates.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace Services {

class ProvisioningService {
public:
    static ProvisioningService& getInstance();

    void begin();
    
    // Check if WiFi is connected and provisioned
    bool isProvisioned() const;

    // Reset credentials and enter provisioning mode
    void resetCredentials();

    // Manually request QR mode for new Wi-Fi
    void requestQRMode();

    // The BLE callbacks will call these
    void handlePin(const String& pin);
    void handleSSID(const String& ssid);
    void handlePassword(const String& password);
    void handleBLEConnect();
    void handleBLEDisconnect(int reason);

private:
    ProvisioningService() = default;
    ~ProvisioningService() = default;
    ProvisioningService(const ProvisioningService&) = delete;
    ProvisioningService& operator=(const ProvisioningService&) = delete;

    void updateState(App::ProvisioningState newState);
    static void serviceTask(void* pvParameters);
    void runService();
    void doWifiConnect();
    void buildDeviceInfo();

    TaskHandle_t _taskHandle = nullptr;
    App::ProvisioningState _currentState = App::ProvisioningState::None;
    
    bool _pinVerified = false;
    String _receivedSSID = "";
    String _receivedPass = "";
    String _connectedSSID = "";
    String _connectedIP = "";
    String _deviceInfo = "";

    bool _pendingConnect = false;
    bool _pendingDisconnect = false;
    
    bool _userRequestedQR = false;
    uint32_t _lastRetryTime = 0;
};

} // namespace Services

#endif // SERVICES_PROVISIONING_SERVICE_H
