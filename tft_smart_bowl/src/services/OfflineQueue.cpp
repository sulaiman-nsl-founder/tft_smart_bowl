#include "services/OfflineQueue.h"
#include "services/RecordStore.h"
#include "services/Logger.h"
#include <Arduino.h>

namespace Services {

static const char* QUEUE_FILE = "/queue.dat";

OfflineQueue& OfflineQueue::getInstance() {
    static OfflineQueue instance;
    return instance;
}

void OfflineQueue::begin() {
    loadFromDisk();

    // Subscribe to events that should be persisted offline.
    EventBus::getInstance().subscribe([](const SystemEvent& event) {
        OfflineQueue::getInstance().handleEvent(event);
    });
}

void OfflineQueue::handleEvent(const SystemEvent& event) {
    // Only persist critical events
    if (event.id != EventId::FeedingSessionEnded && 
        event.id != EventId::BowlStateChanged &&
        event.id != EventId::ErrorAsserted) {
        return;
    }

    if (_count >= MAX_QUEUE_SIZE) {
        LOG_WARN("Q", 850, "Offline queue full! Dropping oldest event.");
        _head = (_head + 1) % MAX_QUEUE_SIZE;
        _count--;
    }

    _queue[_tail] = event;
    _tail = (_tail + 1) % MAX_QUEUE_SIZE;
    _count++;

    // For simplicity in this milestone, we flush the whole queue to disk on every event.
    // A robust system would buffer and occasionally flush, or append individually.
    saveToDisk();
}

size_t OfflineQueue::getPendingCount() const {
    return _count;
}

Core::Result<SystemEvent> OfflineQueue::peek() {
    if (_count == 0) {
        return Core::Result<SystemEvent>(Core::Error(
            Core::ErrorCategory::System, Core::ErrorCode::InvalidState, "Queue empty"));
    }
    return Core::Result<SystemEvent>(_queue[_head]);
}

void OfflineQueue::acknowledge() {
    if (_count > 0) {
        _head = (_head + 1) % MAX_QUEUE_SIZE;
        _count--;
        saveToDisk();
    }
}

void OfflineQueue::clear() {
    _head = 0;
    _tail = 0;
    _count = 0;
    RecordStore::getInstance().writeAtomic(QUEUE_FILE, nullptr, 0);
}

void OfflineQueue::saveToDisk() {
    // Serialize current queue items sequentially
    if (_count == 0) {
        RecordStore::getInstance().writeAtomic(QUEUE_FILE, nullptr, 0);
        return;
    }

    // Allocate buffer for all events
    size_t totalSize = _count * sizeof(SystemEvent);
    uint8_t* buffer = new uint8_t[totalSize];
    
    size_t index = _head;
    for (size_t i = 0; i < _count; i++) {
        memcpy(&buffer[i * sizeof(SystemEvent)], &_queue[index], sizeof(SystemEvent));
        index = (index + 1) % MAX_QUEUE_SIZE;
    }

    RecordStore::getInstance().writeAtomic(QUEUE_FILE, buffer, totalSize);
    delete[] buffer;
}

void OfflineQueue::loadFromDisk() {
    _head = 0;
    _tail = 0;
    _count = 0;

    auto callback = [](const uint8_t* data, size_t length) {
        if (length % sizeof(SystemEvent) != 0) {
            LOG_ERROR("Q", 851, "Corrupted queue file payload length");
            return;
        }

        size_t count = length / sizeof(SystemEvent);
        auto& q = OfflineQueue::getInstance();
        
        for (size_t i = 0; i < count; i++) {
            if (q._count < MAX_QUEUE_SIZE) {
                SystemEvent evt;
                memcpy(&evt, data + (i * sizeof(SystemEvent)), sizeof(SystemEvent));
                q._queue[q._tail] = evt;
                q._tail = (q._tail + 1) % MAX_QUEUE_SIZE;
                q._count++;
            }
        }
    };

    auto res = RecordStore::getInstance().readAllRecords(QUEUE_FILE, callback);
    if (res.isOk()) {
        LOG_INFO("Q", 852, "Loaded %u offline events", _count);
    }
}

} // namespace Services
