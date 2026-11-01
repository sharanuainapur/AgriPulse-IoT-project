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
    // dallas.setWaitForConversion(true);
    Serial.print("Temperature sensors found: ");
    Serial.println(dallas.getDeviceCount());
    logInfo("TempSensor", "Initialized for DS18B20");
}
bool TemperatureSensor::readCelsius(float& result) {
    dallas.requestTemperatures();
    int tempSensorCount = dallas.getDeviceCount();
    if (tempSensorCount == 0) {
        dallas.begin();
        Serial.print("Temperature sensors found while reading: ");
        Serial.println(tempSensorCount);
    }
    const float value = dallas.getTempCByIndex(0);
    if (value == DEVICE_DISCONNECTED_C || value < -55.0f || value > 125.0f) {
        return false;
    }
    result = value;
    return true;
}
