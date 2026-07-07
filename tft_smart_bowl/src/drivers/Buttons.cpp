#include "drivers/Buttons.h"
#include "drivers/Mcp23017.h"
#include "board/Pins.h"
#include "services/Logger.h"
#include <Arduino.h>

namespace Drivers {

Buttons& Buttons::getInstance() {
    static Buttons instance;
    return instance;
}

void Buttons::begin() {
    auto& mcp = Mcp23017::getInstance();
    if (!mcp.isInitialized()) {
        LOG_ERROR("BTN", 500, "Cannot init buttons: MCP23017 not initialized");
        return;
    }

    // Configure buttons as inputs with pull-ups (reference code pattern)
    mcp.setPinMode(Board::Pins::McpPortA::BUTTON_1, INPUT_PULLUP);
    mcp.setPinMode(Board::Pins::McpPortA::BUTTON_2, INPUT_PULLUP);
    mcp.setPinMode(Board::Pins::McpPortA::BUTTON_3, INPUT_PULLUP);

    // Initialize state
    for (int i = 0; i < NUM_BUTTONS; i++) {
        _buttons[i] = ButtonState();
    }

    _initialized = true;
    LOG_INFO("BTN", 501, "Buttons initialized (3 buttons on MCP Port A)");
}

void Buttons::update() {
    if (!_initialized) return;

    auto& mcp = Mcp23017::getInstance();
    uint32_t now = millis();

    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
        ButtonState& btn = _buttons[i];

        // Read raw state (active LOW with pull-up, so invert)
        bool raw = !mcp.digitalRead(i);
        btn.rawState = raw;

        // Debounce
        if (raw != btn.debouncedState) {
            if ((now - btn.lastChangeTime) >= DEBOUNCE_MS) {
                btn.lastDebouncedState = btn.debouncedState;
                btn.debouncedState = raw;
                btn.lastChangeTime = now;

                if (btn.debouncedState && !btn.lastDebouncedState) {
                    // Rising edge (pressed)
                    btn.pendingEvent = ButtonEvent::Press;
                    btn.pressStartTime = now;
                    btn.longPressFired = false;
                    btn.lastRepeatTime = now;
                } else if (!btn.debouncedState && btn.lastDebouncedState) {
                    // Falling edge (released)
                    btn.pendingEvent = ButtonEvent::Release;
                }
            }
        } else {
            btn.lastChangeTime = now;
        }

        // Long press and repeat detection (while held)
        if (btn.debouncedState) {
            uint32_t heldDuration = now - btn.pressStartTime;

            if (!btn.longPressFired && heldDuration >= LONG_PRESS_MS) {
                btn.pendingEvent = ButtonEvent::LongPress;
                btn.longPressFired = true;
                btn.lastRepeatTime = now;
            } else if (btn.longPressFired && (now - btn.lastRepeatTime) >= REPEAT_MS) {
                btn.pendingEvent = ButtonEvent::Repeat;
                btn.lastRepeatTime = now;
            }
        }
    }
}

ButtonEvent Buttons::getEvent(ButtonId id) {
    uint8_t idx = static_cast<uint8_t>(id);
    if (idx >= NUM_BUTTONS) return ButtonEvent::None;

    ButtonEvent evt = _buttons[idx].pendingEvent;
    _buttons[idx].pendingEvent = ButtonEvent::None;
    return evt;
}

bool Buttons::isPressed(ButtonId id) const {
    uint8_t idx = static_cast<uint8_t>(id);
    if (idx >= NUM_BUTTONS) return false;
    return _buttons[idx].debouncedState;
}

} // namespace Drivers
