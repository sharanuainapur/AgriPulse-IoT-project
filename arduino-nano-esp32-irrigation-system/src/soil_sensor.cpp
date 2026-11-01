#include "soil_sensor.h"
#include "config.h"
#include "logger.h"

void SoilSensor::begin()
{
    pinMode(SOIL_SENSOR_PIN, INPUT);
    analogReadResolution(12);
    logInfo("SoilSensor", "Initialized (12-bit ADC)");
}

int SoilSensor::readRaw() const
{
    constexpr int samples = 20;
    long total = 0;

    for (int i = 0; i < samples; ++i)
    {
        total += analogRead(SOIL_SENSOR_PIN);
        delay(10);
    }

    return static_cast<int>(total / samples);
}

float SoilSensor::toMoisturePercent(int raw) const
{
    const float dry = static_cast<float>(SOIL_ADC_DRY);
    const float wet = static_cast<float>(SOIL_ADC_WET);

    if (dry == wet)
    {
        logError("SoilSensor", "Invalid calibration values");
        return -1.0f;
    }

    const float moisture =
        100.0f * (dry - static_cast<float>(raw)) / (dry - wet);

    return constrain(moisture, 0.0f, 100.0f);
}
