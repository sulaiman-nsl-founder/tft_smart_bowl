#include "services/EventBus.h"
#include "services/Logger.h"
#include "services/Supervisor.h"
#include <Arduino.h>

namespace Services {

EventBus& EventBus::getInstance() {
    static EventBus instance;
    return instance;
}

void EventBus::begin(size_t queueDepth) {
    if (_queue == nullptr) {
        _queue = xQueueCreate(queueDepth, sizeof(SystemEvent));
        if (_queue == nullptr) {
            LOG_ERROR("EVT", 700, "Failed to create EventBus queue");
        } else {
            LOG_INFO("EVT", 701, "EventBus initialized (depth %u)", queueDepth);
            
            xTaskCreatePinnedToCore(
                dispatcherTask,
                "EventBus",
                4096,
                this,
                4, // Priority same as UI
                &_dispatcherTaskHandle,
                1  // App core
            );
        }
    }
}

void EventBus::subscribe(EventCallback callback) {
    if (_subscriberCount < MAX_SUBSCRIBERS && callback != nullptr) {
        _subscribers[_subscriberCount++] = callback;
    }
}

bool EventBus::publish(const SystemEvent& event) {
    if (_queue == nullptr) return false;

    // Use a very short timeout so we don't block critical tasks for long
    if (xQueueSend(_queue, &event, pdMS_TO_TICKS(10)) != pdTRUE) {
        _overflowCount++;
        LOG_WARN("EVT", 702, "EventBus queue full (ID: %u). Overflows: %u", 
                 (uint16_t)event.id, _overflowCount);
        return false;
    }
    return true;
}

bool EventBus::publishFromIsr(const SystemEvent& event, bool* higherPriorityTaskWoken) {
    if (_queue == nullptr) return false;

    BaseType_t woken = pdFALSE;
    if (xQueueSendFromISR(_queue, &event, &woken) != pdTRUE) {
        _overflowCount++;
        return false;
    }

    if (higherPriorityTaskWoken != nullptr && woken == pdTRUE) {
        *higherPriorityTaskWoken = true;
    }
    return true;
}

bool EventBus::poll(SystemEvent* outEvent, uint32_t timeoutMs) {
    if (_queue == nullptr || outEvent == nullptr) return false;

    return (xQueueReceive(_queue, outEvent, pdMS_TO_TICKS(timeoutMs)) == pdTRUE);
}

uint32_t EventBus::getOverflowCount() const {
    return _overflowCount;
}

void EventBus::dispatcherTask(void* pvParameters) {
    EventBus* bus = static_cast<EventBus*>(pvParameters);
    Supervisor::getInstance().registerCurrentTask();
    bus->runDispatcher();
}

void EventBus::runDispatcher() {
    SystemEvent event;
    while (true) {
        Supervisor::getInstance().feed();
        
        if (xQueueReceive(_queue, &event, pdMS_TO_TICKS(100)) == pdTRUE) {
            for (size_t i = 0; i < _subscriberCount; i++) {
                _subscribers[i](event);
            }
        }
    }
}

} // namespace Services
