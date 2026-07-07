#include "drivers/Leds.h"
#include "drivers/Mcp23017.h"
#include "board/Pins.h"
#include "services/Logger.h"
#include <Arduino.h>

namespace Drivers {

Leds& Leds::getInstance() {
    static Leds instance;
    return instance;
}

uint8_t Leds::mcpPin(LedId id) const {
    switch (id) {
        case LedId::Led1: return Board::Pins::McpPortB::LED_1;
        case LedId::Led2: return Board::Pins::McpPortB::LED_2;
        case LedId::Led3: return Board::Pins::McpPortB::LED_3;
        default: return Board::Pins::McpPortB::LED_1;
    }
}

void Leds::begin() {
    auto& mcp = Mcp23017::getInstance();
    if (!mcp.isInitialized()) {
        LOG_ERROR("LED", 600, "Cannot init LEDs: MCP23017 not initialized");
        return;
    }

    // Configure LED pins as outputs (reference code pattern)
    mcp.setPinMode(Board::Pins::McpPortB::LED_1, OUTPUT);
    mcp.setPinMode(Board::Pins::McpPortB::LED_2, OUTPUT);
    mcp.setPinMode(Board::Pins::McpPortB::LED_3, OUTPUT);

    // All LEDs off initially
    allOff();

    _initialized = true;
    LOG_INFO("LED", 601, "LEDs initialized (3 LEDs on MCP Port B)");
}

void Leds::applyOutput(LedId id, bool state) {
    auto& mcp = Mcp23017::getInstance();
    if (!mcp.isInitialized()) return;

    uint8_t idx = static_cast<uint8_t>(id);
    if (idx >= NUM_LEDS) return;

    _leds[idx].outputState = state;
    mcp.digitalWrite(mcpPin(id), state);
}

void Leds::update() {
    if (!_initialized) return;

    uint32_t now = millis();

    for (uint8_t i = 0; i < NUM_LEDS; i++) {
        LedState& led = _leds[i];
        LedId id = static_cast<LedId>(i);

        switch (led.pattern) {
            case LedPattern::Off:
                if (led.outputState) applyOutput(id, false);
                break;

            case LedPattern::On:
                if (!led.outputState) applyOutput(id, true);
                break;

            case LedPattern::BlinkSlow:
                if ((now - led.lastToggle) >= 500) {
                    applyOutput(id, !led.outputState);
                    led.lastToggle = now;
                }
                break;

            case LedPattern::BlinkFast:
                if ((now - led.lastToggle) >= 100) {
                    applyOutput(id, !led.outputState);
                    led.lastToggle = now;
                }
                break;

            case LedPattern::Pulse:
                if (led.outputState && (now - led.pulseStart) >= 200) {
                    applyOutput(id, false);
                    led.pattern = LedPattern::Off;
                }
                break;
        }
    }
}

void Leds::set(LedId id, LedPattern pattern) {
    uint8_t idx = static_cast<uint8_t>(id);
    if (idx >= NUM_LEDS) return;

    _leds[idx].pattern = pattern;
    _leds[idx].lastToggle = millis();

    if (pattern == LedPattern::Pulse) {
        _leds[idx].pulseStart = millis();
        applyOutput(id, true);
    } else if (pattern == LedPattern::On) {
        applyOutput(id, true);
    } else if (pattern == LedPattern::Off) {
        applyOutput(id, false);
    }
}

void Leds::on(LedId id) {
    set(id, LedPattern::On);
}

void Leds::off(LedId id) {
    set(id, LedPattern::Off);
}

void Leds::allOff() {
    for (uint8_t i = 0; i < NUM_LEDS; i++) {
        set(static_cast<LedId>(i), LedPattern::Off);
    }
}

bool Leds::isOn(LedId id) const {
    uint8_t idx = static_cast<uint8_t>(id);
    if (idx >= NUM_LEDS) return false;
    return _leds[idx].outputState;
}

} // namespace Drivers
