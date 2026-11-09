#include "pump_controller.h"
#include "config.h"
#include "logger.h"

static uint8_t outputLevel(bool on, bool activeLow)
{
    return (on != activeLow) ? HIGH : LOW;
}

void PumpController::begin()
{
    // Write the OFF level before switching to OUTPUT so the pin never glitches ON.
    digitalWrite(PUMP_RELAY_PIN, outputLevel(false, PUMP_ACTIVE_LOW));
    digitalWrite(MOTOR_LED_PIN, outputLevel(false, MOTOR_LED_ACTIVE_LOW));
    pinMode(PUMP_RELAY_PIN, OUTPUT);
    pinMode(MOTOR_LED_PIN, OUTPUT);

    pumpOn_ = false;
    logInfo("Pump", "Safe boot state: PUMP OFF");
}

void PumpController::setPump(bool on, const char *reason)
{
    if (pumpOn_ == on)
        return;

    pumpOn_ = on;
    digitalWrite(PUMP_RELAY_PIN, outputLevel(on, PUMP_ACTIVE_LOW));
    digitalWrite(MOTOR_LED_PIN, outputLevel(on, MOTOR_LED_ACTIVE_LOW));

    logInfo("Pump", on ? "PUMP ON" : "PUMP OFF");
    logInfo("Pump", reason);
}
