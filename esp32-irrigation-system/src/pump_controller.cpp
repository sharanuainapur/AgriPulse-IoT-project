#include "pump_controller.h"
#include "config.h"
#include "logger.h"

// void PumpController::begin()
// {
//     pinMode(PUMP_RELAY_PIN, OUTPUT);
//     pinMode(MOTOR_LED_PIN, OUTPUT);
//     setPump(false, "safe boot state");
// }
// void PumpController::setPump(bool on, const char *reason)
// {
//     if (pumpOn_ == on)
//         return;
//     pumpOn_ = on;
//     digitalWrite(PUMP_RELAY_PIN, (on != PUMP_ACTIVE_LOW) ? HIGH : LOW);
//     digitalWrite(MOTOR_LED_PIN, (on != MOTOR_LED_ACTIVE_LOW) ? HIGH : LOW);
//     logInfo("Pump", on ? "PUMP ON" : "PUMP OFF");
//     logInfo("Pump", reason);
// }

void PumpController::begin()
{
    pinMode(PUMP_RELAY_PIN, OUTPUT);
    pinMode(MOTOR_LED_PIN, OUTPUT);

    // Active-LOW relay: HIGH means OFF
    digitalWrite(PUMP_RELAY_PIN, HIGH);
    digitalWrite(MOTOR_LED_PIN, LOW);

    pumpOn_ = false;
    logInfo("Pump", "Safe boot state: PUMP OFF");
}

void PumpController::setPump(bool on, const char *reason)
{
    if (pumpOn_ == on)
        return;

    pumpOn_ = on;

    // Active-LOW relay
    digitalWrite(PUMP_RELAY_PIN, on ? LOW : HIGH);

    // Assuming the motor LED is active HIGH
    digitalWrite(MOTOR_LED_PIN, on ? HIGH : LOW);

    logInfo("Pump", on ? "PUMP ON" : "PUMP OFF");
    logInfo("Pump", reason);
}
