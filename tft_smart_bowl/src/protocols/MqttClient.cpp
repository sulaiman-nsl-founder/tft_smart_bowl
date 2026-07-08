#include "protocols/MqttClient.h"
#include "services/SecureStore.h"
#include "services/Logger.h"
#include "platform/BuildInfo.h"
#include <WiFi.h>
#include <time.h>
#include <ArduinoJson.h>

namespace Protocols {

MqttClient::MqttClient() : _pubSub(_secureClient) {
}

MqttClient& MqttClient::getInstance() {
    static MqttClient instance;
    return instance;
}

void MqttClient::begin() {
    syncTimeForTls();
    _pubSub.setCallback(mqttCallback);
}

void MqttClient::syncTimeForTls() {
    LOG_INFO("MQTT", 300, "Configuring SNTP for TLS time sync...");
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
}

void MqttClient::update() {
    if (WiFi.status() != WL_CONNECTED) return;
    
    auto& store = Services::SecureStore::getInstance();
    if (!store.hasValidMqttIdentity() || !store.hasValidPemMaterials()) {
        return; // wait until provisioned
    }

    if (!_pubSub.connected()) {
        if (millis() - _lastConnectAttempt > 5000) {
            _lastConnectAttempt = millis();
            
            time_t now = time(nullptr);
            if (now < 1700000000) {
                LOG_WARN("MQTT", 301, "Time not synced yet. MQTT TLS cannot connect.");
                return;
            }
            
            connect();
        }
    } else {
        _pubSub.loop();
    }
}

void MqttClient::connect() {
    auto& store = Services::SecureStore::getInstance();
    
    _secureClient.setCACert(store.getRootCa().c_str());
    _secureClient.setCertificate(store.getClientCert().c_str());
    _secureClient.setPrivateKey(store.getPrivateKey().c_str());
    _secureClient.setTimeout(10);
    
    _pubSub.setServer(store.getMqttHost().c_str(), store.getMqttPort());
    
    String lwtTopic = store.getMqttBaseTopic() + "/status/offline";
    
    StaticJsonDocument<512> doc;
    doc["msg_type"] = "status";
    doc["device_uuid"] = store.getDeviceUuid();
    doc["serial_number"] = store.getDeviceSerialNumber();
    doc["connection_status"] = "offline";
    doc["reason"] = "lwt";
    
    String lwtPayload;
    serializeJson(doc, lwtPayload);
    
    LOG_INFO("MQTT", 303, "Connecting to %s:%d as %s", store.getMqttHost().c_str(), store.getMqttPort(), store.getMqttClientId().c_str());
    
    if (_pubSub.connect(store.getMqttClientId().c_str(), nullptr, nullptr, lwtTopic.c_str(), 1, false, lwtPayload.c_str())) {
        LOG_INFO("MQTT", 304, "Connected to MQTT broker");
        String otaTopic = store.getMqttBaseTopic() + "/ota/cmd";
        _pubSub.subscribe(otaTopic.c_str());
        
        // Let CloudSync know we connected so it can send status/online
    } else {
        LOG_ERROR("MQTT", 305, "MQTT connect failed, rc=%d", _pubSub.state());
    }
}

bool MqttClient::publish(const String& topic, const String& payload, bool retained) {
    if (!_pubSub.connected()) return false;
    bool ok = _pubSub.publish(topic.c_str(), (const uint8_t*)payload.c_str(), payload.length(), retained);
    if (ok) {
        LOG_DEBUG("MQTT", 306, "Published to %s", topic.c_str());
    } else {
        LOG_ERROR("MQTT", 307, "Failed to publish to %s", topic.c_str());
    }
    return ok;
}

bool MqttClient::subscribe(const String& topic) {
    if (!_pubSub.connected()) return false;
    return _pubSub.subscribe(topic.c_str());
}

bool MqttClient::isConnected() {
    return _pubSub.connected();
}

void MqttClient::mqttCallback(char* topic, byte* payload, unsigned int length) {
    String payloadStr;
    payloadStr.reserve(length + 1);
    for (unsigned int i = 0; i < length; i++) {
        payloadStr += (char)payload[i];
    }
    LOG_INFO("MQTT", 308, "Received %s: %s", topic, payloadStr.c_str());
}

} // namespace Protocols
