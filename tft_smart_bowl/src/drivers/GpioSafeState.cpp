#include "drivers/GpioSafeState.h"
#include "board/Pins.h"
#include <Arduino.h>
#include <Wire.h>

namespace Board {

void GpioSafeState::initialize() {
    // 1. ESP32 Direct GPIO Safe States
    
    // SPI Chip Selects (Active LOW, so set HIGH for inactive)
    pinMode(Pins::TFT_CS, OUTPUT);
    digitalWrite(Pins::TFT_CS, HIGH);

    pinMode(Pins::SD_CS, OUTPUT);
    digitalWrite(Pins::SD_CS, HIGH);

    // TFT Control
    pinMode(Pins::TFT_RST, OUTPUT);
    digitalWrite(Pins::TFT_RST, LOW); // Hold in reset initially

    pinMode(Pins::TFT_DC, OUTPUT);
    digitalWrite(Pins::TFT_DC, LOW);

    // Shutdown Control
    pinMode(Pins::SHUTDN_CTRL, OUTPUT);
    digitalWrite(Pins::SHUTDN_CTRL, LOW); // Assuming LOW is shutdown/safe state

    // 2. MCP23017 Safe States via I2C
    // The MCP23017 powers up with all pins as inputs (floating). 
    // To prevent any unintended pulses when they are later configured as outputs,
    // we explicitly set the output latches (OLATA/OLATB) to a safe state,
    // and configure the required pins as outputs immediately.
    
    Wire.begin(Pins::I2C_SDA, Pins::I2C_SCL);
    
    const uint8_t MCP23017_ADDRESS = 0x20; // Default address if A0-A2 are grounded
    
    // IOCON.BANK = 0 by default.
    // Register addresses:
    // IODIRA = 0x00, IODIRB = 0x01
    // OLATA = 0x14, OLATB = 0x15

    // Set OLATA and OLATB to 0x00 (all outputs driven LOW when enabled)
    // Note: CAM_PWDN (GPB6) might need to be HIGH to power down, but if we don't 
    // know the exact polarity, leaving it as input (floating) or driving LOW is a starting point.
    // We will set them to 0 and leave them as inputs for now, so they don't drive anything 
    // until the explicit driver takes over, but setting latches to 0 prevents a HIGH glitch.
    Wire.beginTransmission(MCP23017_ADDRESS);
    Wire.write(0x14); // Start at OLATA
    Wire.write(0x00); // OLATA = 0x00
    Wire.write(0x00); // OLATB = 0x00 (auto-incremented address 0x15)
    Wire.endTransmission();

    // Optionally, we could set IODIR to make them outputs now, but 
    // since the drivers will do that, just setting the latches is sufficient 
    // to prevent glitches.
    
    // We are done with the raw I2C, release it so the I2cBus manager can take it later.
    // Wire.end() is not always cleanly implemented, but calling Wire.begin() again later is usually fine.
}

} // namespace Board
