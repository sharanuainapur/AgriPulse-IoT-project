#pragma once
#include <Arduino.h>
#include "app_state.h"
#include "config.h"

// The part of SharedState the dashboard shows, copied out under the mutex.
struct DisplayStatus {
    SensorSnapshot sensors;
    OperatingMode mode = OperatingMode::AUTO;
    bool wifiConnected = false;
    bool mqttConnected = false;
    bool pumpOn = false;
    PumpFault pumpFault = PumpFault::NONE;
    uint32_t pumpStartedAtMs = 0;
};

class OledDisplay {
public:
    bool begin();
    bool available() const { return available_; }
    void showMessage(const char* line0, const char* line1 = "", const char* line2 = "", const char* line3 = "");
    void showLine(uint8_t row, const char* text);
    void showDashboard(const DisplayStatus& status);
private:
    void showSensorPage(const DisplayStatus& status);
    void showStatusPage(const DisplayStatus& status);
    void drawLine(uint8_t row, const char* text); // updates the frame buffer only
    void flush();                                 // sends the buffer if any line changed
    bool available_ = false;
    bool dirty_ = false;
    char shown_[OLED_ROWS][OLED_COLUMNS + 1] = {};
    uint8_t page_ = 0;
    uint32_t pageShownAtMs_ = 0;
};
