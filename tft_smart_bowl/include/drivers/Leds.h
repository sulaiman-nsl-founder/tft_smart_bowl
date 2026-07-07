#ifndef DRIVERS_LEDS_H
#define DRIVERS_LEDS_H

#include <stdint.h>

namespace Drivers {

enum class LedId : uint8_t {
    Led1 = 0,
    Led2 = 1,
    Led3 = 2,
    Count = 3
};

enum class LedPattern : uint8_t {
    Off = 0,
    On,
    BlinkSlow,   // 500ms on / 500ms off
    BlinkFast,   // 100ms on / 100ms off
    Pulse        // Single flash, then off
};

/**
 * @brief LED driver for 3 indicator LEDs on MCP23017 Port B.
 * 
 * Based on reference code MapButtonsToLEDs_MCP23017.ino.
 */
class Leds {
public:
    static Leds& getInstance();

    /** @brief Initialize LED pins on MCP23017 as outputs, all off. */
    void begin();

    /** @brief Update LED states (handles blink patterns). Call from main loop. */
    void update();

    /** @brief Set an LED to a specific pattern. */
    void set(LedId id, LedPattern pattern);

    /** @brief Turn on an LED (steady). */
    void on(LedId id);

    /** @brief Turn off an LED. */
    void off(LedId id);

    /** @brief Turn off all LEDs. */
    void allOff();

    /** @brief Check if an LED is currently on. */
    bool isOn(LedId id) const;

private:
    Leds() = default;
    ~Leds() = default;
    Leds(const Leds&) = delete;
    Leds& operator=(const Leds&) = delete;

    static constexpr uint8_t NUM_LEDS = 3;

    struct LedState {
        LedPattern pattern   = LedPattern::Off;
        bool       outputState = false;
        uint32_t   lastToggle = 0;
        uint32_t   pulseStart = 0;
    };

    LedState _leds[NUM_LEDS];
    bool _initialized = false;

    void applyOutput(LedId id, bool state);
    uint8_t mcpPin(LedId id) const;
};

} // namespace Drivers

#endif // DRIVERS_LEDS_H
