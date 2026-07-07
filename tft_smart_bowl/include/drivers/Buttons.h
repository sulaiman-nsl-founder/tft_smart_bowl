#ifndef DRIVERS_BUTTONS_H
#define DRIVERS_BUTTONS_H

#include <stdint.h>

namespace Drivers {

/**
 * @brief Button event types.
 */
enum class ButtonEvent : uint8_t {
    None = 0,
    Press,
    Release,
    LongPress,
    Repeat
};

/**
 * @brief Button ID mapping to MCP23017 Port A pins.
 */
enum class ButtonId : uint8_t {
    Button1 = 0,  // GPA0
    Button2 = 1,  // GPA1
    Button3 = 2,  // GPA2
    Count   = 3
};

/**
 * @brief Debounced button driver for 3 buttons on MCP23017 Port A.
 *
 * Provides press, release, long-press, and repeat events.
 * Based on reference code MapButtonsToLEDs_MCP23017.ino.
 */
class Buttons {
public:
    static Buttons& getInstance();

    /**
     * @brief Initialize button pins on the MCP23017.
     * Must be called after Mcp23017::begin().
     */
    void begin();

    /**
     * @brief Poll button states and generate events.
     * Call this from the main loop at a regular interval (e.g., every 10-20ms).
     */
    void update();

    /**
     * @brief Check if a button event is pending.
     * @param id Which button.
     * @return The pending event, or None.
     */
    ButtonEvent getEvent(ButtonId id);

    /**
     * @brief Check if a button is currently pressed (debounced).
     */
    bool isPressed(ButtonId id) const;

    /** @brief Debounce time in ms. */
    static constexpr uint32_t DEBOUNCE_MS     = 30;
    /** @brief Long press threshold in ms. */
    static constexpr uint32_t LONG_PRESS_MS   = 1000;
    /** @brief Repeat interval in ms after long press. */
    static constexpr uint32_t REPEAT_MS       = 200;

private:
    Buttons() = default;
    ~Buttons() = default;
    Buttons(const Buttons&) = delete;
    Buttons& operator=(const Buttons&) = delete;

    static constexpr uint8_t NUM_BUTTONS = 3;

    struct ButtonState {
        bool     rawState      = false;
        bool     debouncedState = false;
        bool     lastDebouncedState = false;
        uint32_t lastChangeTime = 0;
        uint32_t pressStartTime = 0;
        uint32_t lastRepeatTime = 0;
        bool     longPressFired = false;
        ButtonEvent pendingEvent = ButtonEvent::None;
    };

    ButtonState _buttons[NUM_BUTTONS];
    bool _initialized = false;
};

} // namespace Drivers

#endif // DRIVERS_BUTTONS_H
