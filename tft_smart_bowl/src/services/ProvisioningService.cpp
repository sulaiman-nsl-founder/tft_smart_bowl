#include "services/ProvisioningService.h"
#include "services/EventBus.h"
#include "services/Logger.h"
#include "platform/BuildInfo.h"
#include <NimBLEDevice.h>
#include <WiFi.h>
#include <Preferences.h>
#include <Arduino.h>

namespace Services {

static const char* SERVICE_UUID      = "12345678-1234-1234-1234-123456789abc";
static const char* CHAR_PIN_UUID     = "12345678-1234-1234-1234-123456789000";
static const char* CHAR_SSID_UUID    = "12345678-1234-1234-1234-123456789001";
static const char* CHAR_PASS_UUID    = "12345678-1234-1234-1234-123456789002";
static const char* CHAR_STATUS_UUID  = "12345678-1234-1234-1234-123456789003";
static const char* CHAR_DEVINFO_UUID = "12345678-1234-1234-1234-123456789004";

static const char* POP_PIN = "1234";

NimBLECharacteristic* pStatusChar = nullptr;

class PINCallback : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pChar) override {
        String entered(pChar->getValue().c_str());
        ProvisioningService::getInstance().handlePin(entered);
    }
};

class SSIDCallback : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pChar) override {
        String ssid(pChar->getValue().c_str());
        ProvisioningService::getInstance().handleSSID(ssid);
    }
};

class PassCallback : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pChar) override {
        String pass(pChar->getValue().c_str());
        ProvisioningService::getInstance().handlePassword(pass);
    }
};

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) override {
        ProvisioningService::getInstance().handleBLEConnect();
    }
    void onDisconnect(NimBLEServer* pServer) override {
        ProvisioningService::getInstance().handleBLEDisconnect(0); // v1.4 doesn't provide reason here
    }
};

ProvisioningService& ProvisioningService::getInstance() {
    static ProvisioningService instance;
    return instance;
}

void ProvisioningService::begin() {
    buildDeviceInfo();

    xTaskCreatePinnedToCore(
        serviceTask,
        "ProvTask",
        8192,
        this,
        3,
        &_taskHandle,
        1
    );
}

bool ProvisioningService::isProvisioned() const {
    return WiFi.status() == WL_CONNECTED;
}

void ProvisioningService::resetCredentials() {
    Preferences prefs;
    prefs.begin("wifi", false);
    prefs.clear();
    prefs.end();
    
    WiFi.disconnect(true);
    _pinVerified = false;
    _receivedSSID = "";
    _receivedPass = "";
    
    updateState(App::ProvisioningState::QR);
    if (pStatusChar) {
        pStatusChar->setValue("STATE:ADVERTISING");
        pStatusChar->notify();
    }
    NimBLEDevice::startAdvertising();
}

void ProvisioningService::buildDeviceInfo() {
    String mac = WiFi.macAddress();
    String chipModel = ESP.getChipModel();
    uint8_t cores = ESP.getChipCores();
    uint32_t flashSizeMB = ESP.getFlashChipSize() / (1024 * 1024);

    char buf[200];
    snprintf(buf, sizeof(buf),
      "{\"mac\":\"%s\",\"fw\":\"%s\",\"chip\":\"%s\",\"cores\":%d,\"flash\":\"%dMB\",\"ModelID\":\"BOWL.1.0.0\"}",
      mac.c_str(), FW_VERSION, chipModel.c_str(), cores, (int)flashSizeMB);
    
    _deviceInfo = String(buf);
}

void ProvisioningService::updateState(App::ProvisioningState newState) {
    _currentState = newState;
    SystemEvent evt;
    evt.id = EventId::ProvisioningStateChanged;
    evt.timestampMs = millis();
    evt.payload.provisioning.state = static_cast<uint8_t>(newState);
    
    if (newState == App::ProvisioningState::WifiConnected || newState == App::ProvisioningState::AutoConnected) {
        IPAddress ip = WiFi.localIP();
        evt.payload.provisioning.ip[0] = ip[0];
        evt.payload.provisioning.ip[1] = ip[1];
        evt.payload.provisioning.ip[2] = ip[2];
        evt.payload.provisioning.ip[3] = ip[3];
    } else {
        memset(evt.payload.provisioning.ip, 0, 4);
    }
    
    EventBus::getInstance().publish(evt);
}

void ProvisioningService::handlePin(const String& pin) {
    if (pin == POP_PIN) {
        _pinVerified = true;
        if (pStatusChar) pStatusChar->setValue("STATE:PIN_OK");
        if (pStatusChar) pStatusChar->notify();
        updateState(App::ProvisioningState::PinOk);
        
        delay(50);
        if (pStatusChar) pStatusChar->setValue((String("STATE:DEVICE_INFO:") + _deviceInfo).c_str());
        if (pStatusChar) pStatusChar->notify();
    } else {
        _pinVerified = false;
        if (pStatusChar) pStatusChar->setValue("STATE:PIN_FAIL");
        if (pStatusChar) pStatusChar->notify();
        updateState(App::ProvisioningState::PinFail);
    }
}

void ProvisioningService::handleSSID(const String& ssid) {
    if (!_pinVerified) {
        if (pStatusChar) pStatusChar->setValue("STATE:LOCKED");
        if (pStatusChar) pStatusChar->notify();
        updateState(App::ProvisioningState::Locked);
        return;
    }
    _receivedSSID = ssid;
    _connectedSSID = ssid;
    
    char msg[80];
    snprintf(msg, sizeof(msg), "STATE:SSID_RECEIVED:%s", ssid.c_str());
    if (pStatusChar) pStatusChar->setValue(msg);
    if (pStatusChar) pStatusChar->notify();
}

void ProvisioningService::handlePassword(const String& password) {
    if (!_pinVerified) {
        if (pStatusChar) pStatusChar->setValue("STATE:LOCKED");
        if (pStatusChar) pStatusChar->notify();
        updateState(App::ProvisioningState::Locked);
        return;
    }
    _receivedPass = password;
    if (pStatusChar) pStatusChar->setValue("STATE:PASS_RECEIVED");
    if (pStatusChar) pStatusChar->notify();
    updateState(App::ProvisioningState::Sending);
    _pendingConnect = true;
}

void ProvisioningService::handleBLEConnect() {
    if (pStatusChar) pStatusChar->setValue("STATE:BLE_CONNECTED");
    if (pStatusChar) pStatusChar->notify();
    updateState(App::ProvisioningState::PhoneConnected);
    
    // Simulate the sequence in the original sketch
    delay(800);
    updateState(App::ProvisioningState::EnterPin);
    if (pStatusChar) pStatusChar->setValue("STATE:ENTER_PIN");
    if (pStatusChar) pStatusChar->notify();
}

void ProvisioningService::handleBLEDisconnect(int reason) {
    _pinVerified = false;
    _receivedSSID = "";
    _receivedPass = "";
    _pendingDisconnect = true;
}

void ProvisioningService::doWifiConnect() {
    if (_receivedSSID.isEmpty()) {
        if (pStatusChar) pStatusChar->setValue("STATE:WIFI_FAILED:no_ssid");
        if (pStatusChar) pStatusChar->notify();
        updateState(App::ProvisioningState::WifiFailed);
        return;
    }

    updateState(App::ProvisioningState::WifiConnecting);
    if (pStatusChar) pStatusChar->setValue("STATE:WIFI_CONNECTING");
    if (pStatusChar) pStatusChar->notify();
    
    WiFi.disconnect(true);
    WiFi.begin(_receivedSSID.c_str(), _receivedPass.c_str());

    uint8_t tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 20) {
        vTaskDelay(pdMS_TO_TICKS(500)); 
        tries++;
        if (tries % 4 == 0) {
            String msg = String("STATE:WIFI_CONNECTING:attempt=") + tries + "/20";
            if (pStatusChar) pStatusChar->setValue(msg.c_str());
            if (pStatusChar) pStatusChar->notify();
        }
    }

    if (WiFi.status() == WL_CONNECTED) {
        _connectedIP = WiFi.localIP().toString();
        String msg = String("STATE:WIFI_CONNECTED:") + _connectedIP + ":ssid=" + _connectedSSID;
        if (pStatusChar) pStatusChar->setValue(msg.c_str());
        if (pStatusChar) pStatusChar->notify();
        
        updateState(App::ProvisioningState::WifiConnected);
        
        Preferences prefs;
        prefs.begin("wifi", false);
        prefs.putString("ssid", _receivedSSID);
        prefs.putString("pass", _receivedPass);
        prefs.end();
    } else {
        String msg = String("STATE:WIFI_FAILED:ssid=") + _connectedSSID;
        if (pStatusChar) pStatusChar->setValue(msg.c_str());
        if (pStatusChar) pStatusChar->notify();
        updateState(App::ProvisioningState::WifiFailed);
        WiFi.disconnect(true);
    }
}

void ProvisioningService::serviceTask(void* pvParameters) {
    ProvisioningService* service = static_cast<ProvisioningService*>(pvParameters);
    service->runService();
}

void ProvisioningService::runService() {
    // Attempt Auto-connect
    Preferences prefs;
    prefs.begin("wifi", true);
    String savedSSID = prefs.getString("ssid", "");
    String savedPass = prefs.getString("pass", "");
    prefs.end();

    if (!savedSSID.isEmpty()) {
        _connectedSSID = savedSSID;
        updateState(App::ProvisioningState::WifiConnecting);
        WiFi.begin(savedSSID.c_str(), savedPass.c_str());
        
        uint8_t t = 0;
        while (WiFi.status() != WL_CONNECTED && t < 10) {
            vTaskDelay(pdMS_TO_TICKS(500));
            t++;
        }
        if (WiFi.status() == WL_CONNECTED) {
            _connectedIP = WiFi.localIP().toString();
            updateState(App::ProvisioningState::AutoConnected);
        }
    }

    // Initialize BLE
    NimBLEDevice::init("");
    NimBLEDevice::setMTU(185);

    NimBLEServer* pBLEServer = NimBLEDevice::createServer();
    pBLEServer->setCallbacks(new ServerCallbacks());
    NimBLEService* pService = pBLEServer->createService(SERVICE_UUID);

    NimBLECharacteristic* pPINChar = pService->createCharacteristic(CHAR_PIN_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    pPINChar->setCallbacks(new PINCallback());

    NimBLECharacteristic* pSSIDChar = pService->createCharacteristic(CHAR_SSID_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    pSSIDChar->setCallbacks(new SSIDCallback());

    NimBLECharacteristic* pPassChar = pService->createCharacteristic(CHAR_PASS_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    pPassChar->setCallbacks(new PassCallback());

    pStatusChar = pService->createCharacteristic(CHAR_STATUS_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    pStatusChar->setValue("STATE:BOOT");

    NimBLECharacteristic* pDevInfoChar = pService->createCharacteristic(CHAR_DEVINFO_UUID, NIMBLE_PROPERTY::READ);
    pDevInfoChar->setValue(_deviceInfo.c_str());

    pService->start();

    NimBLEAdvertising* pAdv = NimBLEDevice::getAdvertising();
    pAdv->setName("ESP32-WiFi-Setup");
    pAdv->addServiceUUID(SERVICE_UUID);
    pAdv->setScanResponse(true);
    pAdv->start();

    if (WiFi.status() == WL_CONNECTED) {
        updateState(App::ProvisioningState::AutoConnected);
        if (pStatusChar) pStatusChar->setValue((String("STATE:AUTO_CONNECTED:") + _connectedIP).c_str());
        if (pStatusChar) pStatusChar->notify();
    } else {
        updateState(App::ProvisioningState::QR);
        if (pStatusChar) pStatusChar->setValue("STATE:ADVERTISING");
        if (pStatusChar) pStatusChar->notify();
    }

    while (true) {
        if (_pendingDisconnect) {
            _pendingDisconnect = false;
            if (pStatusChar) pStatusChar->setValue("STATE:BLE_DISCONNECTED:reason=0");
            if (pStatusChar) pStatusChar->notify();
            NimBLEDevice::startAdvertising();
            if (WiFi.status() != WL_CONNECTED) {
                updateState(App::ProvisioningState::QR);
                if (pStatusChar) pStatusChar->setValue("STATE:ADVERTISING");
                if (pStatusChar) pStatusChar->notify();
            }
        }

        if (_pendingConnect) {
            _pendingConnect = false;
            doWifiConnect();
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

} // namespace Services
