#include "services/SecureStore.h"
#include "services/Logger.h"
#include <LittleFS.h>
#include <Preferences.h>

namespace Services {

static const char* MQTT_CA_FILE          = "/mqtt_ca.pem";
static const char* MQTT_CLIENT_CERT_FILE = "/mqtt_client_cert.pem";
static const char* MQTT_PRIVATE_KEY_FILE = "/mqtt_private_key.pem";

SecureStore& SecureStore::getInstance() {
    static SecureStore instance;
    return instance;
}

bool SecureStore::begin() {
    if (!LittleFS.begin(true)) {
        LOG_ERROR("SECURE", 100, "Failed to mount LittleFS for SecureStore");
        return false;
    }
    LOG_INFO("SECURE", 101, "SecureStore initialized (LittleFS mounted)");
    return true;
}

String SecureStore::getDeviceSerialNumber() const {
    // Hardcoded for testing as per reference code
    return String("PW-SBWC001-2602-00001");
}

String SecureStore::normalizePem(String pem) const {
    pem.trim();
    pem.replace("\\n", "\n");
    pem.replace("\\r", "");
    return pem;
}

bool SecureStore::isValidCertificatePem(const String& pem) const {
    return pem.indexOf("-----BEGIN CERTIFICATE-----") >= 0 &&
           pem.indexOf("-----END CERTIFICATE-----") >= 0;
}

bool SecureStore::isValidPrivateKeyPem(const String& pem) const {
    return (pem.indexOf("-----BEGIN PRIVATE KEY-----") >= 0 && pem.indexOf("-----END PRIVATE KEY-----") >= 0) ||
           (pem.indexOf("-----BEGIN RSA PRIVATE KEY-----") >= 0 && pem.indexOf("-----END RSA PRIVATE KEY-----") >= 0) ||
           (pem.indexOf("-----BEGIN EC PRIVATE KEY-----") >= 0 && pem.indexOf("-----END EC PRIVATE KEY-----") >= 0);
}

bool SecureStore::writeTextFile(const char* path, const String& content) {
    File f = LittleFS.open(path, "w");
    if (!f) {
        LOG_ERROR("SECURE", 102, "Failed to open %s for write", path);
        return false;
    }
    size_t written = f.print(content);
    f.close();
    if (written != content.length()) {
        LOG_ERROR("SECURE", 103, "Short write on %s (written: %u, expected: %u)", path, written, content.length());
        return false;
    }
    return true;
}

String SecureStore::readTextFile(const char* path) {
    if (!LittleFS.exists(path)) return "";
    
    File f = LittleFS.open(path, "r");
    if (!f) return "";
    
    String content;
    content.reserve(f.size() + 1);
    while (f.available()) {
        content += (char)f.read();
    }
    f.close();
    
    return normalizePem(content);
}

bool SecureStore::hasValidPemMaterials() const {
    return isValidCertificatePem(_rootCa) &&
           isValidCertificatePem(_clientCert) &&
           isValidPrivateKeyPem(_privateKey);
}

bool SecureStore::saveMqttPemMaterials(const String& ca, const String& clientCert, const String& privateKey) {
    _rootCa = normalizePem(ca);
    _clientCert = normalizePem(clientCert);
    _privateKey = normalizePem(privateKey);

    if (!hasValidPemMaterials()) {
        LOG_ERROR("SECURE", 104, "Refusing to save invalid/incomplete MQTT PEM materials");
        return false;
    }

    bool ok = true;
    ok &= writeTextFile(MQTT_CA_FILE, _rootCa);
    ok &= writeTextFile(MQTT_CLIENT_CERT_FILE, _clientCert);
    ok &= writeTextFile(MQTT_PRIVATE_KEY_FILE, _privateKey);

    if (ok) {
        LOG_INFO("SECURE", 105, "MQTT PEM materials saved securely");
    } else {
        LOG_ERROR("SECURE", 106, "Failed to write MQTT PEM materials to LittleFS");
    }
    return ok;
}

bool SecureStore::loadMqttPemMaterials() {
    _rootCa = readTextFile(MQTT_CA_FILE);
    _clientCert = readTextFile(MQTT_CLIENT_CERT_FILE);
    _privateKey = readTextFile(MQTT_PRIVATE_KEY_FILE);

    if (!hasValidPemMaterials()) {
        LOG_WARN("SECURE", 107, "MQTT PEM materials missing or incomplete on disk");
        return false;
    }

    LOG_INFO("SECURE", 108, "MQTT PEM materials loaded successfully");
    return true;
}

void SecureStore::clearPemMaterials() {
    if (LittleFS.exists(MQTT_CA_FILE)) LittleFS.remove(MQTT_CA_FILE);
    if (LittleFS.exists(MQTT_CLIENT_CERT_FILE)) LittleFS.remove(MQTT_CLIENT_CERT_FILE);
    if (LittleFS.exists(MQTT_PRIVATE_KEY_FILE)) LittleFS.remove(MQTT_PRIVATE_KEY_FILE);

    _rootCa = "";
    _clientCert = "";
    _privateKey = "";
    LOG_INFO("SECURE", 109, "MQTT PEM materials cleared");
}

bool SecureStore::hasValidMqttIdentity() const {
    return _deviceUuid.length() > 0 &&
           _mqttHost.length() > 0 &&
           _mqttPort > 0 &&
           _mqttClientId.length() > 0 &&
           _mqttBaseTopic.length() > 0;
}

bool SecureStore::saveMqttIdentity(const String& uuid, const String& host, int port, const String& clientId, const String& baseTopic) {
    _deviceUuid = uuid;
    _mqttHost = host;
    _mqttPort = port;
    _mqttClientId = clientId;
    _mqttBaseTopic = baseTopic;

    if (!hasValidMqttIdentity()) {
        LOG_ERROR("SECURE", 110, "Refusing to save invalid MQTT identity");
        return false;
    }

    Preferences prefs;
    prefs.begin("mqtt_ident", false);
    
    bool ok = true;
    ok &= prefs.putString("serial_number", getDeviceSerialNumber()) > 0;
    ok &= prefs.putString("device_uuid", _deviceUuid) > 0;
    ok &= prefs.putString("mqtt_host", _mqttHost) > 0;
    ok &= prefs.putInt("mqtt_port", _mqttPort) > 0;
    ok &= prefs.putString("mqtt_client_id", _mqttClientId) > 0;
    ok &= prefs.putString("mqtt_base_topic", _mqttBaseTopic) > 0;
    ok &= prefs.putBool("provisioned", true) > 0;

    prefs.end();

    if (ok) {
        LOG_INFO("SECURE", 111, "MQTT Identity saved to NVS");
    } else {
        LOG_ERROR("SECURE", 112, "Failed to save MQTT Identity to NVS");
    }
    return ok;
}

bool SecureStore::loadMqttIdentity() {
    Preferences prefs;
    prefs.begin("mqtt_ident", true);

    bool provisioned = prefs.getBool("provisioned", false);
    String storedSerial = prefs.getString("serial_number", "");

    _deviceUuid = prefs.getString("device_uuid", "");
    _mqttHost = prefs.getString("mqtt_host", "");
    _mqttPort = prefs.getInt("mqtt_port", 8883);
    _mqttClientId = prefs.getString("mqtt_client_id", "");
    _mqttBaseTopic = prefs.getString("mqtt_base_topic", "");

    prefs.end();

    if (!provisioned) {
        return false;
    }

    if (storedSerial != getDeviceSerialNumber()) {
        LOG_WARN("SECURE", 113, "Stored MQTT identity belongs to old serial. Ignoring.");
        return false;
    }

    if (!hasValidMqttIdentity()) {
        LOG_WARN("SECURE", 114, "Stored MQTT identity corrupted or incomplete.");
        return false;
    }

    LOG_INFO("SECURE", 115, "MQTT Identity loaded from NVS");
    return true;
}

void SecureStore::clearMqttIdentity() {
    Preferences prefs;
    prefs.begin("mqtt_ident", false);
    prefs.clear();
    prefs.end();
    
    _deviceUuid = "";
    _mqttHost = "";
    _mqttPort = 0;
    _mqttClientId = "";
    _mqttBaseTopic = "";
    
    LOG_INFO("SECURE", 116, "MQTT Identity cleared from NVS");
}

} // namespace Services
