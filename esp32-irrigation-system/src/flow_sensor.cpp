#include "flow_sensor.h"
#include "config.h"
#include "logger.h"
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>

volatile uint32_t FlowSensor::pulses_ = 0;
volatile uint32_t FlowSensor::totalPulses_ = 0;
volatile uint32_t FlowSensor::lastPulseAtMs_ = 0;

void FlowSensor::begin() {
    pinMode(FLOW_SENSOR_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), FlowSensor::onPulse, FALLING);
    logInfo("FlowSensor", "Pulse interrupt attached");
}
void IRAM_ATTR FlowSensor::onPulse() {
    ++pulses_;
    ++totalPulses_;
    lastPulseAtMs_ = millis();
}
uint32_t FlowSensor::readAndResetPulses() {
    noInterrupts();
    uint32_t value = pulses_;
    pulses_ = 0;
    interrupts();
    return value;
}
uint32_t FlowSensor::totalPulses() const {
    noInterrupts(); uint32_t value = totalPulses_; interrupts(); return value;
}
uint32_t FlowSensor::lastPulseAtMs() const {
    noInterrupts(); uint32_t value = lastPulseAtMs_; interrupts(); return value;
}
