#include <Arduino.h>
#include "temperature_sensor.h"
#include "config.h"
#include "logger.h"
#include <OneWire.h>
#include <DallasTemperature.h>

// #define TEMPERATURE_PIND2 5

static OneWire oneWire(TEMPERATURE_PIN);
static DallasTemperature dallas(&oneWire);

void TemperatureSensor::begin() {
    dallas.begin();
    // Don't let the library busy-poll the bus during conversion; readCelsius() sleeps instead.
    dallas.setWaitForConversion(false);
    const uint8_t count = dallas.getDeviceCount();
    char msg[64];
    snprintf(msg, sizeof(msg), "Initialized for DS18B20; %u sensor(s) found", count);
    if (count == 0)
        logWarn("TempSensor", msg);
    else
        logInfo("TempSensor", msg);
}
bool TemperatureSensor::readCelsius(float& result) {
    dallas.requestTemperatures();
    // delay() yields to FreeRTOS, so the CPU idles for the ~750 ms conversion.
    delay(dallas.millisToWaitForConversion(dallas.getResolution()));
    int tempSensorCount = dallas.getDeviceCount();
    if (tempSensorCount == 0) {
        logWarn("TempSensor", "No DS18B20 found on bus; rescanning");
        dallas.begin();
    }
    const float value = dallas.getTempCByIndex(0);
    if (value == DEVICE_DISCONNECTED_C || value < -55.0f || value > 125.0f) {
        return false;
    }
    result = value;
    return true;
}
