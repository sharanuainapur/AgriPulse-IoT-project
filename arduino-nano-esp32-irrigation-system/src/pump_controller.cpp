#include "pump_controller.h"
#include "config.h"
#include "logger.h"

void PumpController::begin() {
    pinMode(PUMP_RELAY_PIN, OUTPUT);
    pinMode(MOTOR_LED_PIN, OUTPUT);
    setPump(false, "safe boot state");
}
void PumpController::setPump(bool on, const char* reason) {
    if (pumpOn_ == on) return;
    pumpOn_ = on;
    digitalWrite(PUMP_RELAY_PIN, (on != PUMP_ACTIVE_LOW) ? HIGH : LOW);
    digitalWrite(MOTOR_LED_PIN, (on != MOTOR_LED_ACTIVE_LOW) ? HIGH : LOW);
    logInfo("Pump", on ? "PUMP ON" : "PUMP OFF");
    logInfo("Pump", reason);
}
