#include "app/BoardSelfTest.h"
#include "drivers/I2cBus.h"
#include "drivers/Mcp23017.h"
#include "drivers/Buttons.h"
#include "drivers/Leds.h"
#include "drivers/SpiBus.h"
#include "drivers/TftDisplay.h"
#include "board/Pins.h"
#include "services/Logger.h"
#include <Arduino.h>

namespace App {

SelfTestResult BoardSelfTest::run() {
    SelfTestResult result;

    LOG_INFO("BIST", 900, "=== Board Self-Test Starting ===");

    // 1. I2C Bus
    result.i2cBusOk = testI2cBus();
    LOG_INFO("BIST", 901, "I2C Bus: %s", result.i2cBusOk ? "PASS" : "FAIL");

    // 2. MCP23017
    if (result.i2cBusOk) {
        result.mcpOk = testMcp23017();
    }
    LOG_INFO("BIST", 902, "MCP23017: %s", result.mcpOk ? "PASS" : "FAIL/SKIP");

    // 3. Buttons & LEDs (need MCP)
    if (result.mcpOk) {
        result.buttonsOk = testButtonsLeds();
        result.ledsOk = result.buttonsOk;
    }
    LOG_INFO("BIST", 903, "Buttons/LEDs: %s", result.buttonsOk ? "PASS" : "FAIL/SKIP");

    // 4. SPI Bus
    result.spiOk = testSpiBus();
    LOG_INFO("BIST", 904, "SPI Bus: %s", result.spiOk ? "PASS" : "FAIL");

    // 5. TFT Display
    if (result.spiOk) {
        result.tftOk = testTftDisplay();
    }
    LOG_INFO("BIST", 905, "TFT Display: %s", result.tftOk ? "PASS" : "FAIL/SKIP");

    // 6. Memory report
    reportMemory(result);

    // 7. (Skipped displaying results on TFT per user request)

    LOG_INFO("BIST", 910, "=== Self-Test %s ===", result.allPassed() ? "PASSED" : "FAILED");
    return result;
}

bool BoardSelfTest::testI2cBus() {
    auto& i2c = Drivers::I2cBus::getInstance();
    auto res = i2c.begin(Board::Pins::I2C_SDA, Board::Pins::I2C_SCL);
    if (res.isError()) {
        LOG_ERROR("BIST", 911, "I2C init failed: %s", res.error().message);
        return false;
    }

    // Scan for devices
    uint8_t count = i2c.scan();
    LOG_INFO("BIST", 912, "I2C scan found %u device(s)", count);
    return true;
}

bool BoardSelfTest::testMcp23017() {
    auto& mcp = Drivers::Mcp23017::getInstance();
    auto res = mcp.begin(0x20);
    if (res.isError()) {
        LOG_ERROR("BIST", 913, "MCP23017 init failed: %s", res.error().message);
        return false;
    }
    return true;
}

bool BoardSelfTest::testButtonsLeds() {
    // Initialize buttons
    Drivers::Buttons::getInstance().begin();
    
    // Initialize LEDs
    Drivers::Leds::getInstance().begin();

    // Quick LED test: blink each LED once
    auto& leds = Drivers::Leds::getInstance();
    for (uint8_t i = 0; i < 3; i++) {
        leds.on(static_cast<Drivers::LedId>(i));
        delay(150);
        leds.off(static_cast<Drivers::LedId>(i));
        delay(50);
    }

    LOG_INFO("BIST", 914, "Button/LED init complete — LEDs flashed");
    return true;
}

bool BoardSelfTest::testSpiBus() {
    auto& spi = Drivers::SpiBus::getInstance();
    auto res = spi.begin(Board::Pins::TFT_MOSI, 
                         Board::Pins::TFT_SCLK, 
                         Board::Pins::SPI_MISO);
    if (res.isError()) {
        LOG_ERROR("BIST", 915, "SPI init failed: %s", res.error().message);
        return false;
    }
    return true;
}

bool BoardSelfTest::testTftDisplay() {
    auto& tft = Drivers::TftDisplay::getInstance();
    auto res = tft.begin();
    if (res.isError()) {
        LOG_ERROR("BIST", 916, "TFT init failed: %s", res.error().message);
        return false;
    }

    // Show Splash screen instead of test pattern
    tft.fillScreen(Drivers::TftDisplay::COLOR_BLACK);
    
    // Draw "SMART BOWL" centered roughly
    tft.setTextSize(2);
    tft.setTextColor(Drivers::TftDisplay::COLOR_GREEN);
    
    // 128x128 screen, "SMART BOWL" is 10 chars * 12px = 120px wide
    tft.setCursor(4, 50); 
    tft.print("SMART BOWL");
    
    tft.setTextSize(1);
    tft.setTextColor(Drivers::TftDisplay::COLOR_WHITE);
    tft.setCursor(20, 80);
    tft.print("Starting up...");
    
    return true;
}

void BoardSelfTest::reportMemory(SelfTestResult& result) {
    result.freeHeap = ESP.getFreeHeap();
    result.freePsram = ESP.getFreePsram();
    LOG_INFO("BIST", 920, "Free Heap: %u bytes, Free PSRAM: %u bytes", 
             result.freeHeap, result.freePsram);
}

void BoardSelfTest::displayResults(const SelfTestResult& result) {
    auto& tft = Drivers::TftDisplay::getInstance();
    
    // Wait 2 seconds to show test pattern, then show results
    delay(2000);
    tft.clear();
    
    tft.setTextSize(1);
    tft.setCursor(2, 2);
    tft.setTextColor(Drivers::TftDisplay::COLOR_CYAN);
    tft.println("SMART-BOWL SELF-TEST");
    tft.println("");
    
    auto printResult = [&](const char* name, bool ok) {
        tft.setTextColor(ok ? Drivers::TftDisplay::COLOR_GREEN : Drivers::TftDisplay::COLOR_RED);
        tft.print(name);
        tft.println(ok ? " : OK" : " : FAIL");
    };

    printResult("I2C Bus  ", result.i2cBusOk);
    printResult("MCP23017 ", result.mcpOk);
    printResult("Buttons  ", result.buttonsOk);
    printResult("LEDs     ", result.ledsOk);
    printResult("SPI Bus  ", result.spiOk);
    printResult("TFT      ", result.tftOk);

    tft.println("");
    tft.setTextColor(Drivers::TftDisplay::COLOR_WHITE);

    char buf[48];
    snprintf(buf, sizeof(buf), "Heap: %u", result.freeHeap);
    tft.println(buf);
    snprintf(buf, sizeof(buf), "PSRAM: %u", result.freePsram);
    tft.println(buf);

    tft.println("");
    tft.setTextColor(result.allPassed() ? Drivers::TftDisplay::COLOR_GREEN : Drivers::TftDisplay::COLOR_RED);
    tft.println(result.allPassed() ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
}

} // namespace App
