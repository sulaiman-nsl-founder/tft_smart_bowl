#pragma once
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

namespace Protocols {

class MqttClient {
public:
    static MqttClient& getInstance();
    
    void begin();
    void update();
    
    bool publish(const String& topic, const String& payload, bool retained = false);
    bool subscribe(const String& topic);
    
    bool isConnected();

private:
    MqttClient();
    
    void connect();
    void syncTimeForTls();
    static void mqttCallback(char* topic, byte* payload, unsigned int length);

    WiFiClientSecure _secureClient;
    PubSubClient _pubSub;
    uint32_t _lastConnectAttempt = 0;
};

} // namespace Protocols
