#ifndef BOARD_PINS_H
#define BOARD_PINS_H

#include <stdint.h>

namespace Board {
namespace Pins {

// TFT Display Pins (FSPI)
constexpr uint8_t TFT_CS = 47;
constexpr uint8_t TFT_DC = 14;
constexpr uint8_t TFT_RST = 21;
constexpr uint8_t TFT_MOSI = 38;
constexpr uint8_t TFT_SCLK = 39;
constexpr uint8_t SPI_MISO = 40; // Optional/shared

// I2C Bus Pins
constexpr uint8_t I2C_SDA = 42;
constexpr uint8_t I2C_SCL = 41;

// MCP23017 IO Expander Interrupt
constexpr uint8_t MCP_INTA = 0;

// SD Card Chip Select (under FSPI)
constexpr uint8_t SD_CS = 45;

// Pushbuttons and Control
constexpr uint8_t RESET_PB = 4;
constexpr uint8_t SHUTDN_CTRL = 46;

// Analog Sensors
constexpr uint8_t BATT_ADC = 1;
constexpr uint8_t LDR_ADC = 2;

// Service UART (UART0)
constexpr uint8_t UART_TX = 43;
constexpr uint8_t UART_RX = 44;

// Camera interface (except PCLK which is deferred due to conflict with TFT_DC
// GPIO 14)
constexpr uint8_t CAM_SIOD = 5;
constexpr uint8_t CAM_SIOC = 6;
constexpr uint8_t CAM_VSYNC = 7;
constexpr uint8_t CAM_HREF = 15;
constexpr uint8_t CAM_XCLK = 16;
constexpr uint8_t CAM_D0 = 12;
constexpr uint8_t CAM_D1 = 10;
constexpr uint8_t CAM_D2 = 9;
constexpr uint8_t CAM_D3 = 11;
constexpr uint8_t CAM_D4 = 13;
constexpr uint8_t CAM_D5 = 8;
constexpr uint8_t CAM_D6 = 18;
constexpr uint8_t CAM_D7 = 17;
// constexpr uint8_t CAM_PCLK = ... // Deferred until resolved

// MCP23017 Port A Pin Indexes (0 to 7)
namespace McpPortA {
constexpr uint8_t BUTTON_1 = 0; // GPA0
constexpr uint8_t BUTTON_2 = 1; // GPA1
constexpr uint8_t BUTTON_3 = 2; // GPA2
constexpr uint8_t PIR_SENS = 3; // GPA3
constexpr uint8_t PWR_BTN = 4;  // GPA4
} // namespace McpPortA

// MCP23017 Port B Pin Indexes (8 to 15 when using Adafruit MCP23X17 library)
namespace McpPortB {
constexpr uint8_t LED_1 = 10;     // GPB0
constexpr uint8_t LED_2 = 9;      // GPB1
constexpr uint8_t LED_3 = 8;      // GPB2
constexpr uint8_t IR_CUT_1 = 11;  // GPB3
constexpr uint8_t IR_CUT_2 = 12;  // GPB4
constexpr uint8_t CAM_RESET = 13; // GPB5
constexpr uint8_t CAM_PWDN = 14;  // GPB6
constexpr uint8_t BUZZER = 15;    // GPB7
} // namespace McpPortB

// Pin definitions are manually verified to be unique.

} // namespace Pins
} // namespace Board

#endif // BOARD_PINS_H
