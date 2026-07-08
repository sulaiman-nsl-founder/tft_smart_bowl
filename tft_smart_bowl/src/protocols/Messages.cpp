#include "protocols/Messages.h"
#include "services/SecureStore.h"
#include "platform/BuildInfo.h"
#include <WiFi.h>
#include <ArduinoJson.h>

namespace Protocols {
namespace Messages {

String makeOnlineStatus() {
    StaticJsonDocument<768> doc;
    auto& store = Services::SecureStore::getInstance();

    doc["msg_type"]           = "status";
    doc["device_uuid"]        = store.getDeviceUuid();
    doc["serial_number"]      = store.getDeviceSerialNumber();
    doc["product_code"]       = "smartbowl";
    doc["model_code"]         = "sbwc001";
    doc["connection_status"]  = "online";
    doc["fw_version"]         = FW_VERSION;
    doc["hw_version"]         = "HW-V1.0";
    doc["mac_address"]        = WiFi.macAddress();
    doc["ip"]                 = WiFi.localIP().toString();
    doc["rssi"]               = WiFi.RSSI();
    doc["sleep_status"]       = "awake";
    doc["uptime_ms"]          = millis();

    String payload;
    serializeJson(doc, payload);
    return payload;
}

String makeTelemetry(uint32_t seq, int battery, int weight) {
    StaticJsonDocument<768> doc;
    auto& store = Services::SecureStore::getInstance();

    doc["msg_type"]              = "telemetry";
    doc["device_uuid"]           = store.getDeviceUuid();
    doc["serial_number"]         = store.getDeviceSerialNumber();
    doc["product_code"]          = "smartbowl";
    doc["model_code"]            = "sbwc001";
    doc["fw_version"]            = FW_VERSION;
    doc["mac_address"]           = WiFi.macAddress();
    
    doc["seq"]                   = seq;
    doc["uptime_ms"]             = millis();
    doc["connection_status"]     = "online";
    doc["sleep_status"]          = "awake";
    
    doc["battery_level"]         = battery;
    doc["battery_status"]        = battery > 85 ? "good" : (battery > 20 ? "medium" : "low");
    doc["rssi"]                  = WiFi.RSSI();
    doc["heap_free"]             = ESP.getFreeHeap();
    
    doc["actual_weight"]         = weight;
    doc["tracked_weight"]        = weight;
    doc["weight_unit"]           = "grams";
    
    doc["stability_status"]      = "stable";
    doc["bowl_status"]           = weight < 60 ? "low_food" : "food_present";

    String payload;
    serializeJson(doc, payload);
    return payload;
}

} // namespace Messages
} // namespace Protocols
