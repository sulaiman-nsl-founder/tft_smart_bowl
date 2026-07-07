#ifndef SERVICES_SUPERVISOR_H
#define SERVICES_SUPERVISOR_H

#include <stdint.h>

namespace Services {

/**
 * @brief Task heartbeat and watchdog supervisor.
 *
 * Monitors registered tasks via periodic heartbeats.
 * If a task fails to report within its deadline, the supervisor
 * can log it and eventually trigger a system reset via the
 * ESP32 Task Watchdog Timer (TWDT).
 */
class Supervisor {
public:
    static Supervisor& getInstance();

    /**
     * @brief Initialize the ESP32 Task Watchdog Timer.
     * @param timeoutMs Watchdog timeout in milliseconds.
     *                  If no feed happens within this window, the device resets.
     */
    void begin(uint32_t timeoutMs = 5000);

    /**
     * @brief Register the current FreeRTOS task with the TWDT.
     * Must be called from the task that needs supervision.
     */
    void registerCurrentTask();

    /**
     * @brief Unregister the current FreeRTOS task from the TWDT.
     */
    void unregisterCurrentTask();

    /**
     * @brief Feed (kick) the watchdog for the current task.
     * Call this periodically from each registered task's loop.
     */
    void feed();

    /**
     * @brief Check if the supervisor has been initialized.
     */
    bool isActive() const;

private:
    Supervisor() = default;
    ~Supervisor() = default;
    Supervisor(const Supervisor&) = delete;
    Supervisor& operator=(const Supervisor&) = delete;

    bool _active = false;
    uint32_t _timeoutMs = 5000;
};

} // namespace Services

#endif // SERVICES_SUPERVISOR_H
