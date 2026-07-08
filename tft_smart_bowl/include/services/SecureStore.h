#pragma once
#include <Arduino.h>

namespace Services {

class SecureStore {
public:
    static SecureStore& getInstance();
    
    bool begin();
    
    // PEM credentials
    bool saveMqttPemMaterials(const String& ca, const String& clientCert, const String& privateKey);
    bool loadMqttPemMaterials();
    bool hasValidPemMaterials() const;
    void clearPemMaterials();

    const String& getRootCa() const { return _rootCa; }
    const String& getClientCert() const { return _clientCert; }
    const String& getPrivateKey() const { return _privateKey; }

    // MQTT Identity
    bool saveMqttIdentity(const String& uuid, const String& host, int port, const String& clientId, const String& baseTopic);
    bool loadMqttIdentity();
    bool hasValidMqttIdentity() const;
    void clearMqttIdentity();

    const String& getDeviceUuid() const { return _deviceUuid; }
    const String& getMqttHost() const { return _mqttHost; }
    int getMqttPort() const { return _mqttPort; }
    const String& getMqttClientId() const { return _mqttClientId; }
    const String& getMqttBaseTopic() const { return _mqttBaseTopic; }

    String getDeviceSerialNumber() const;

private:
    SecureStore() = default;
    
    bool writeTextFile(const char* path, const String& content);
    String readTextFile(const char* path);
    bool isValidCertificatePem(const String& pem) const;
    bool isValidPrivateKeyPem(const String& pem) const;
    String normalizePem(String pem) const;

    String _rootCa;
    String _clientCert;
    String _privateKey;

    String _deviceUuid;
    String _mqttHost;
    int _mqttPort = 0;
    String _mqttClientId;
    String _mqttBaseTopic;
};

} // namespace Services
