#pragma once
#include <Arduino.h>
class TemperatureSensor {
public:
    void begin();
    bool readCelsius(float& result);
};
