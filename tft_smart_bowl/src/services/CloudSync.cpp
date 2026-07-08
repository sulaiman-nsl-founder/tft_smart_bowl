#include "services/CloudSync.h"
#include "protocols/MqttClient.h"
#include "protocols/Messages.h"
#include "services/WeightService.h"
#include "services/SecureStore.h"
#include "services/Logger.h"

namespace Services {

CloudSync& CloudSync::getInstance() {
    static CloudSync instance;
    return instance;
}

void CloudSync::begin() {
    xTaskCreatePinnedToCore(
        taskRoutine,
        "CloudSync",
        8192,
        this,
        1,
        &_taskHandle,
        1
    );
}

void CloudSync::taskRoutine(void* pvParameters) {
    auto* self = static_cast<CloudSync*>(pvParameters);
    self->run();
}

void CloudSync::run() {
    while (true) {
        auto& mqtt = Protocols::MqttClient::getInstance();
        mqtt.update();
        
        bool isConnected = mqtt.isConnected();
        
        if (isConnected && !_wasConnected) {
            LOG_INFO("SYNC", 400, "MQTT just connected. Publishing online status...");
            String statusPayload = Protocols::Messages::makeOnlineStatus();
            String topic = SecureStore::getInstance().getMqttBaseTopic() + "/status/online";
            mqtt.publish(topic, statusPayload, true);
        }
        
        _wasConnected = isConnected;

        if (isConnected) {
            uint32_t now = millis();
            if (now - _lastTelemetryMs >= 5000) {
                _lastTelemetryMs = now;
                _seq++;
                
                int weight = WeightService::getInstance().getWeight();
                // Battery is just a placeholder for now since there's no battery service yet
                int battery = 100;
                
                String telPayload = Protocols::Messages::makeTelemetry(_seq, battery, weight);
                String telTopic = SecureStore::getInstance().getMqttBaseTopic() + "/telemetry";
                
                mqtt.publish(telTopic, telPayload, false);
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

} // namespace Services
