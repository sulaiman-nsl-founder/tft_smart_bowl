#ifndef SERVICES_EVENT_BUS_H
#define SERVICES_EVENT_BUS_H

#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "core/Result.h"

namespace Services {

/**
 * @brief Identifiers for all system events.
 */
enum class EventId : uint16_t {
    None = 0,
    ButtonPress,
    WeightUpdated,
    TareCompleted,
    SpanCompleted,
    BowlStateChanged,
    FeedingSessionStarted,
    FeedingSessionEnded,
    ErrorAsserted,
    ProvisioningStateChanged
};

/**
 * @brief Fixed-size generic event payload.
 */
struct SystemEvent {
    EventId id;
    uint32_t timestampMs;
    
    // Payload union to keep the event size fixed and small
    union {
        struct {
            uint8_t buttonId;
            uint8_t eventType;
        } button;
        
        struct {
            float grams;
            bool isStable;
        } weight;
        
        struct {
            uint8_t oldState;
            uint8_t newState;
        } bowlState;
        
        struct {
            float amountGrams;
        } feeding;
        
        struct {
            uint8_t state;
            uint8_t ip[4];
        } provisioning;
    } payload;
};

/**
 * @brief Central event bus for decoupled inter-task communication.
 * 
 * Uses a single FreeRTOS queue. Producers push to it, and a dedicated 
 * dispatcher task (or the UI task) pops from it and distributes to subscribers.
 * For now, we will expose a simple queue that the main application/UI can poll.
 */
class EventBus {
public:
    typedef void (*EventCallback)(const SystemEvent&);

    static EventBus& getInstance();

    /**
     * @brief Initialize the event bus queue and dispatcher task.
     * @param queueDepth Maximum number of events in flight (default 32).
     */
    void begin(size_t queueDepth = 32);

    /**
     * @brief Register a callback for all events.
     */
    void subscribe(EventCallback callback);

    /**
     * @brief Publish an event to the bus.
     * @param event The event to publish.
     * @return true if successfully queued, false if queue is full.
     */
    bool publish(const SystemEvent& event);

    /**
     * @brief Publish an event to the bus from an ISR.
     * @param event The event to publish.
     * @return true if successfully queued, false if queue is full.
     */
    bool publishFromIsr(const SystemEvent& event, bool* higherPriorityTaskWoken);

    /**
     * @brief Poll the bus for the next event.
     * @param outEvent Pointer to store the received event.
     * @param timeoutMs Time to wait for an event.
     * @return true if an event was received.
     */
    bool poll(SystemEvent* outEvent, uint32_t timeoutMs = 0);

    /**
     * @brief Get the number of times the queue has overflowed.
     */
    uint32_t getOverflowCount() const;

private:
    EventBus() = default;
    ~EventBus() = default;
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    static void dispatcherTask(void* pvParameters);
    void runDispatcher();

    QueueHandle_t _queue = nullptr;
    TaskHandle_t _dispatcherTaskHandle = nullptr;
    uint32_t _overflowCount = 0;

    static constexpr size_t MAX_SUBSCRIBERS = 8;
    EventCallback _subscribers[MAX_SUBSCRIBERS] = {nullptr};
    size_t _subscriberCount = 0;
};

} // namespace Services

#endif // SERVICES_EVENT_BUS_H
