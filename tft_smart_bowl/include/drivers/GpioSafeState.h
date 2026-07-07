#ifndef BOARD_GPIO_SAFE_STATE_H
#define BOARD_GPIO_SAFE_STATE_H

namespace Board {

class GpioSafeState {
public:
    /**
     * @brief Initialize all critical output pins to a safe inactive state.
     * 
     * This prevents unintended pulses to LEDs, buzzer, camera, and IR controls
     * during boot, before their respective drivers are fully initialized.
     */
    static void initialize();
};

} // namespace Board

#endif // BOARD_GPIO_SAFE_STATE_H
