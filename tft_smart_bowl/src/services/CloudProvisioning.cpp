#include "services/CloudProvisioning.h"
#include "services/SecureStore.h"
#include "services/Logger.h"
#include "platform/BuildInfo.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

namespace Services {

static const char* BACKEND_BASE_URL = "https://smartpetdev.ckdigital.in";
static const char* PROVISION_API_PATH = "/api/v1/device/provision-test";
static const char* PRODUCT_CODE = "smartbowl";
static const char* MODEL_CODE = "sbwc001";

CloudProvisioning& CloudProvisioning::getInstance() {
    static CloudProvisioning instance;
    return instance;
}

void CloudProvisioning::begin() {
    xTaskCreatePinnedToCore(
        taskRoutine,
        "CloudProv",
        8192,
        this,
        2,
        &_taskHandle,
        1
    );
}

void CloudProvisioning::taskRoutine(void* pvParameters) {
    auto* self = static_cast<CloudProvisioning*>(pvParameters);
    self->run();
}

void CloudProvisioning::run() {
    while (true) {
        if (!_provisioned && WiFi.status() == WL_CONNECTED) {
            auto& store = SecureStore::getInstance();
            if (store.loadMqttIdentity() && store.loadMqttPemMaterials()) {
                LOG_INFO("CLOUD_PROV", 200, "Already provisioned with backend. Keys loaded.");
                _provisioned = true;
            } else {
                LOG_INFO("CLOUD_PROV", 201, "Not provisioned yet. Calling backend API...");
                if (doProvisioning()) {
                    _provisioned = true;
                    LOG_INFO("CLOUD_PROV", 202, "Provisioning successful.");
                } else {
                    LOG_WARN("CLOUD_PROV", 203, "Provisioning failed. Retrying in 10 seconds...");
                    vTaskDelay(pdMS_TO_TICKS(10000));
                    continue;
                }
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

bool CloudProvisioning::doProvisioning() {
    String url = String(BACKEND_BASE_URL) + PROVISION_API_PATH;

    StaticJsonDocument<512> req;
    req["serial_number"] = SecureStore::getInstance().getDeviceSerialNumber();
    req["product_code"] = PRODUCT_CODE;
    req["model_code"] = MODEL_CODE;
    req["mac_address"] = WiFi.macAddress();
    req["fw_version"] = FW_VERSION;
    req["hw_version"] = "HW-V1.0";
    
    String requestBody;
    serializeJson(req, requestBody);
    
    WiFiClientSecure httpsClient;
    httpsClient.setInsecure(); // Phase 1 normal HTTPS
    httpsClient.setTimeout(15);
    
    HTTPClient http;
    http.setTimeout(20000);
    
    if (!http.begin(httpsClient, url)) {
        LOG_ERROR("CLOUD_PROV", 204, "http.begin failed");
        return false;
    }
    
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.POST(requestBody);
    String responseBody = http.getString();
    http.end();
    
    if (httpCode <= 0) {
        LOG_ERROR("CLOUD_PROV", 205, "HTTPS transport failed: %d", httpCode);
        return false;
    }
    
    if (httpCode < 200 || httpCode >= 300) {
        LOG_WARN("CLOUD_PROV", 206, "Non-2xx response: %d. Body: %s", httpCode, responseBody.c_str());
    }
    
    return parseResponse(responseBody);
}

bool CloudProvisioning::parseResponse(const String& responseBody) {
    DynamicJsonDocument doc(32768); // 32KB for PEMs
    DeserializationError err = deserializeJson(doc, responseBody);
    if (err) {
        LOG_ERROR("CLOUD_PROV", 207, "JSON parse failed: %s", err.c_str());
        return false;
    }
    
    String status = doc["status"] | "";
    String errorCode = doc["error_code"] | "";
    
    bool usableStatus = (status == "success") || (errorCode == "ALREADY_PROVISIONED");
    if (!usableStatus) {
        LOG_ERROR("CLOUD_PROV", 208, "Provisioning rejected by server");
        return false;
    }
    
    String apiDeviceUuid = doc["device"]["device_uuid"] | "";
    
    String apiHost = doc["mqtt"]["broker_host"] | "";
    if (apiHost.length() == 0) apiHost = doc["mqtt"]["mqtt_host"] | "";
    
    int apiPort = doc["mqtt"]["broker_port"] | 8883;
    if (doc["mqtt"]["mqtt_port"].is<int>()) apiPort = doc["mqtt"]["mqtt_port"] | 8883;
    
    String apiClientId = doc["mqtt"]["mqtt_client_id"] | "";
    if (apiClientId.length() == 0) apiClientId = doc["mqtt"]["client_id"] | "";
    
    String apiBaseTopic = doc["mqtt"]["mqtt_base_topic"] | "";
    if (apiBaseTopic.length() == 0) apiBaseTopic = doc["mqtt"]["base_topic"] | "";
    
    // PEM Fields extraction
    String apiRootCa = doc["credentials"]["root_ca_pem"] | "";
    if (apiRootCa.length() == 0) apiRootCa = doc["credentials"]["mqtt_root_ca_pem"] | "";
    if (apiRootCa.length() == 0) apiRootCa = doc["mqtt"]["ca_pem"] | "";
    
    String apiClientCert = doc["credentials"]["device_client_certificate_pem"] | "";
    if (apiClientCert.length() == 0) apiClientCert = doc["credentials"]["mqtt_client_cert_pem"] | "";
    if (apiClientCert.length() == 0) apiClientCert = doc["mqtt"]["client_cert_pem"] | "";
    
    String apiPrivateKey = doc["credentials"]["device_private_key_pem"] | "";
    if (apiPrivateKey.length() == 0) apiPrivateKey = doc["credentials"]["mqtt_private_key_pem"] | "";
    if (apiPrivateKey.length() == 0) apiPrivateKey = doc["mqtt"]["client_key_pem"] | "";
    
    apiDeviceUuid.trim();
    apiHost.trim();
    apiClientId.trim();
    apiBaseTopic.trim();
    
    if (apiDeviceUuid.length() == 0 || apiHost.length() == 0 || apiClientId.length() == 0 || apiBaseTopic.length() == 0) {
        LOG_ERROR("CLOUD_PROV", 209, "Missing required MQTT identity fields");
        return false;
    }
    
    auto& store = SecureStore::getInstance();
    
    // Fallback: force port to 8883 if the backend sends 1883 for TLS
    if (apiPort != 8883) {
        apiPort = 8883;
    }
    
    if (!store.saveMqttPemMaterials(apiRootCa, apiClientCert, apiPrivateKey)) {
        LOG_ERROR("CLOUD_PROV", 210, "Failed to validate/save PEM materials");
        return false;
    }
    
    if (!store.saveMqttIdentity(apiDeviceUuid, apiHost, apiPort, apiClientId, apiBaseTopic)) {
        LOG_ERROR("CLOUD_PROV", 211, "Failed to save MQTT identity");
        return false;
    }
    
    return true;
}

} // namespace Services
