#pragma once
#include <Arduino.h>
class PumpController {
public:
    void begin();
    void setPump(bool on, const char* reason);
    bool isOn() const { return pumpOn_; }
private:
    bool pumpOn_ = false;
};
