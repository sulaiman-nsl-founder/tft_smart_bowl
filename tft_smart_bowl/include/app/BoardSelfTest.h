#ifndef APP_BOARD_SELF_TEST_H
#define APP_BOARD_SELF_TEST_H

#include <stdint.h>

namespace App {

/**
 * @brief Board-level self-test results.
 */
struct SelfTestResult {
    bool i2cBusOk      = false;
    bool mcpOk         = false;
    bool buttonsOk     = false;
    bool ledsOk        = false;
    bool spiOk         = false;
    bool tftOk         = false;
    uint32_t freeHeap  = 0;
    uint32_t freePsram = 0;
    uint8_t  i2cDevices = 0;

    bool allPassed() const {
        return i2cBusOk && mcpOk && spiOk && tftOk;
        // Buttons and LEDs are secondary — logged but not blocking
    }
};

/**
 * @brief Runs a comprehensive board self-test at boot.
 *
 * Tests all peripherals without hanging if any device is missing.
 * Logs results to the serial console and prints a summary on TFT.
 */
class BoardSelfTest {
public:
    /**
     * @brief Run the full self-test sequence.
     * @return Test results structure.
     */
    static SelfTestResult run();

private:
    static bool testI2cBus();
    static bool testMcp23017();
    static bool testButtonsLeds();
    static bool testSpiBus();
    static bool testTftDisplay();
    static void reportMemory(SelfTestResult& result);
    static void displayResults(const SelfTestResult& result);
};

} // namespace App

#endif // APP_BOARD_SELF_TEST_H
