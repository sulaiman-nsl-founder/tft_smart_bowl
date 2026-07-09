#ifndef APP_UI_CALIBRATION_SCREEN_H
#define APP_UI_CALIBRATION_SCREEN_H

#include "app/ui/Screen.h"
#include <WString.h>

namespace App {
namespace Ui {

/**
 * @brief Guided calibration wizard screen.
 * 
 * Provides a 2-step calibration wizard (Tare + Span).
 */
class CalibrationScreen : public Screen {
public:
    static CalibrationScreen& getInstance();

    void onEnter() override;
    void onExit() override;
    void onUpdate() override;
    bool onEvent(const Services::SystemEvent& event) override;

private:
    CalibrationScreen() = default;
    ~CalibrationScreen() = default;
    CalibrationScreen(const CalibrationScreen&) = delete;
    CalibrationScreen& operator=(const CalibrationScreen&) = delete;

    enum class Step : uint8_t {
        TarePrompt,     // "Remove items, press OK"
        Taring,         // Waiting for tare stability
        SpanPrompt,     // "Place 100g weight, press OK"
        Spanning,       // Waiting for span stability
        Done,           // "Calibration complete!"
        Error           // "Failed - not stable"
    };

    void drawStep();
    void drawTitleBar(const char* title, uint16_t bgColor, uint16_t textColor);
    void drawCentered(const char* text, uint8_t y, uint8_t size, uint16_t color);

    Step _step = Step::TarePrompt;
    bool _needsRedraw = true;
    uint32_t _doneTimer = 0;      // Auto-return timer after calibration done
};

} // namespace Ui
} // namespace App

#endif // APP_UI_CALIBRATION_SCREEN_H
