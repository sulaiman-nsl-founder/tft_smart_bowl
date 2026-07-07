#ifndef SERVICES_OFFLINE_QUEUE_H
#define SERVICES_OFFLINE_QUEUE_H

#include <stdint.h>
#include <stddef.h>
#include "services/EventBus.h"
#include "core/Result.h"

namespace Services {

/**
 * @brief Manages the offline event queue.
 * 
 * Subscribes to the EventBus. When the system is offline, it saves critical 
 * events to the SD card. When online, it replays them.
 */
class OfflineQueue {
public:
    static OfflineQueue& getInstance();

    void begin();

    /**
     * @brief Get the number of pending offline events.
     */
    size_t getPendingCount() const;

    /**
     * @brief Peek at the oldest pending event.
     */
    Core::Result<SystemEvent> peek();

    /**
     * @brief Acknowledge the oldest pending event and remove it from the queue.
     */
    void acknowledge();

    /**
     * @brief Clear the entire queue (e.g., if corrupted or factory reset).
     */
    void clear();

private:
    OfflineQueue() = default;
    ~OfflineQueue() = default;
    OfflineQueue(const OfflineQueue&) = delete;
    OfflineQueue& operator=(const OfflineQueue&) = delete;

    void handleEvent(const SystemEvent& event);
    void saveToDisk();
    void loadFromDisk();

    static constexpr size_t MAX_QUEUE_SIZE = 64; // In RAM cache
    SystemEvent _queue[MAX_QUEUE_SIZE];
    size_t _head = 0; // Read index
    size_t _tail = 0; // Write index
    size_t _count = 0;
};

} // namespace Services

#endif // SERVICES_OFFLINE_QUEUE_H
