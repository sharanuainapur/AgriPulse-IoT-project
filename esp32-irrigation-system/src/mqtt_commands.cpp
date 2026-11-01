#include "mqtt_commands.h"
#include "config.h"
#include "app_state.h"
#include "logger.h"
#include <ArduinoJson.h>

extern SharedState gState;

void handleMqttCommand_(char *topic, byte *payload, unsigned int length)
{
    if (topic == nullptr || payload == nullptr)
    {
        logWarn("MQTT", "Null topic or payload ignored");
        return;
    }

    if (strcmp(topic, MQTT_COMMAND_TOPIC) != 0)
    {
        return;
    }

    // Prevent unexpectedly large MQTT messages from consuming memory.
    constexpr size_t MAX_COMMAND_SIZE = 512;
    if (length == 0 || length > MAX_COMMAND_SIZE)
    {
        logWarn("MQTT", "Empty or oversized command ignored");
        return;
    }

    String body;
    body.reserve(length + 1);

    for (unsigned int i = 0; i < length; ++i)
    {
        body += static_cast<char>(payload[i]);
    }

    Serial.printf(
        "[%10lu ms] [INFO ] [MQTT     ] Command received topic=%s payload=%s\n",
        static_cast<unsigned long>(millis()),
        topic,
        body.c_str());

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);

    if (err)
    {
        logWarn("MQTT", "Invalid JSON command ignored");
        return;
    }

    if (!doc.is<JsonObject>())
    {
        logWarn("MQTT", "Command must be a JSON object");
        return;
    }

    // Parse the mode without modifying shared state.
    bool hasMode = doc["mode"].is<const char *>();
    bool validMode = false;
    OperatingMode requestedMode = OperatingMode::AUTO;

    if (hasMode)
    {
        const char *mode = doc["mode"].as<const char *>();

        if (strcmp(mode, "AUTO") == 0)
        {
            requestedMode = OperatingMode::AUTO;
            validMode = true;
        }
        else if (strcmp(mode, "MANUAL") == 0)
        {
            requestedMode = OperatingMode::MANUAL;
            validMode = true;
        }
        else
        {
            logWarn("MQTT", "Invalid mode; expected AUTO or MANUAL");
        }
    }
    else if (!doc["mode"].isNull() && !doc["mode"].is<const char *>())
    {
        logWarn("MQTT", "Mode must be a string");
    }

    const bool hasPump = doc["pump"].is<bool>();
    const bool validPump = doc["pump"].is<const char *>();

    if (hasPump && !validPump)
    {
        logWarn("MQTT", "Pump command must be a string");
    }

    bool modeApplied = false;
    bool pumpApplied = false;
    bool pumpIgnored = false;

    xSemaphoreTake(gState.mutex, portMAX_DELAY);

    gState.pumpFault = PumpFault::NONE;
    // Apply the mode first, independent of JSON property order.
    if (validMode)
    {
        gState.mode = requestedMode;

        // Clear stale manual requests when entering AUTO.
        if (requestedMode == OperatingMode::AUTO)
        {
            gState.manualPumpRequested = false;
        }

        modeApplied = true;
    }

    // Apply the pump request only if the effective mode is MANUAL.
    if (hasPump && validPump)
    {
        if (gState.mode == OperatingMode::MANUAL)
        {
            const char *pump = doc["pump"].as<const char *>();

            if (strcmp(pump, "ON") == 0)
            {
                gState.manualPumpRequested = true;
            }
            else
            {
                gState.manualPumpRequested = false;
            }
            pumpApplied = true;
        }
        else
        {
            pumpIgnored = true;
        }
    }

    xSemaphoreGive(gState.mutex);

    if (modeApplied)
    {
        logInfo("MQTT",
                requestedMode == OperatingMode::AUTO
                    ? "Mode set to AUTO"
                    : "Mode set to MANUAL");
    }

    if (pumpApplied)
    {

        const char *pump = doc["pump"].as<const char *>();
        logInfo("MQTT", strcmp(pump, "ON") == 0
                            ? "Manual pump request set to ON"
                            : "Manual pump request set to OFF");
    }

    if (pumpIgnored)
    {
        logWarn("MQTT", "Pump command ignored unless mode is MANUAL");
    }

    if (!modeApplied && !pumpApplied && !pumpIgnored)
    {
        logWarn("MQTT", "No supported command applied");
    }
}

void handleMqttCommand(char *topic, byte *payload, unsigned int length)
{
    constexpr size_t MAX_COMMAND_SIZE = 512;

    // 1. Validate MQTT callback arguments.
    if (topic == nullptr || payload == nullptr)
    {
        logWarn("MQTT", "Null topic or payload ignored");
        return;
    }

    if (strcmp(topic, MQTT_COMMAND_TOPIC) != 0)
    {
        return;
    }

    if (length == 0 || length > MAX_COMMAND_SIZE)
    {
        logWarn("MQTT", "Empty or oversized command ignored");
        return;
    }

    // 2. Parse JSON directly from the MQTT buffer.
    JsonDocument doc;

    const DeserializationError error =
        deserializeJson(doc, payload, length);

    if (error)
    {
        logWarn("MQTT", "Invalid JSON command ignored");
        return;
    }

    if (!doc.is<JsonObject>())
    {
        logWarn("MQTT", "Command must be a JSON object");
        return;
    }

    // 3. Validate mode without modifying shared state.
    bool hasMode = false;
    OperatingMode requestedMode = OperatingMode::AUTO;

    if (doc["mode"].is<const char *>())
    {
        hasMode = true;

        const char *mode = doc["mode"].as<const char *>();

        if (strcmp(mode, "AUTO") == 0)
        {
            requestedMode = OperatingMode::AUTO;
        }
        else if (strcmp(mode, "MANUAL") == 0)
        {
            requestedMode = OperatingMode::MANUAL;
        }
        else
        {
            logWarn("MQTT", "Invalid mode; expected AUTO or MANUAL");
            return;
        }
    }
    else if (!doc["mode"].isNull())
    {
        logWarn("MQTT", "Mode must be a string");
        return;
    }

    // 4. Validate pump command.
    bool hasPump = false;
    bool requestedPump = false;

    if (doc["pump"].is<const char *>())
    {
        hasPump = true;

        const char *pump = doc["pump"].as<const char *>();

        if (strcmp(pump, "ON") == 0)
        {
            requestedPump = true;
        }
        else if (strcmp(pump, "OFF") == 0)
        {
            requestedPump = false;
        }
        else
        {
            logWarn("MQTT", "Invalid pump command; expected ON or OFF");
            return;
        }
    }
    else if (!doc["pump"].isNull())
    {
        logWarn("MQTT", "Pump command must be a string");
        return;
    }

    // 5. Reject empty commands.
    if (!hasMode && !hasPump)
    {
        logWarn("MQTT", "No supported command found");
        return;
    }

    // 6. Apply validated changes to shared state.
    bool modeApplied = false;
    bool pumpApplied = false;
    bool pumpIgnored = false;

    xSemaphoreTake(gState.mutex, portMAX_DELAY);
    gState.pumpFault = PumpFault::NONE;

    // Determine the effective mode before applying the pump request.
    const OperatingMode effectiveMode =
        hasMode ? requestedMode : gState.mode;

    if (hasMode)
    {
        gState.mode = requestedMode;

        // Clear stale manual requests when entering AUTO.
        if (requestedMode == OperatingMode::AUTO)
        {
            gState.manualPumpRequested = false;
        }

        modeApplied = true;
    }

    if (hasPump)
    {
        if (effectiveMode == OperatingMode::MANUAL)
        {
            gState.manualPumpRequested = requestedPump;
            pumpApplied = true;
        }
        else
        {
            pumpIgnored = true;
        }
    }

    // Do not clear pumpFault here.
    // Safety faults must be cleared by dedicated safety logic.

    xSemaphoreGive(gState.mutex);

    // 7. Log results outside the critical state update.
    if (modeApplied)
    {
        logInfo("MQTT",
                requestedMode == OperatingMode::AUTO
                    ? "Mode set to AUTO"
                    : "Mode set to MANUAL");
    }

    if (pumpApplied)
    {
        logInfo("MQTT",
                requestedPump
                    ? "Manual pump request set to ON"
                    : "Manual pump request set to OFF");
    }

    if (pumpIgnored)
    {
        logWarn("MQTT", "Pump command ignored unless mode is MANUAL");
    }
}

