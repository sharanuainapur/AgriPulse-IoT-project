#pragma once
#include <Arduino.h>
class FlowSensor {
public:
    void begin();
    uint32_t readAndResetPulses();
    uint32_t totalPulses() const;
    uint32_t lastPulseAtMs() const;
    static void IRAM_ATTR onPulse();
private:
    static volatile uint32_t pulses_;
    static volatile uint32_t totalPulses_;
    static volatile uint32_t lastPulseAtMs_;
};
