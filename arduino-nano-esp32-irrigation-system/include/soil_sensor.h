#pragma once
#include <Arduino.h>
class SoilSensor {
public:
    void begin();
    int readRaw() const;
    float toMoisturePercent(int raw) const;
};
