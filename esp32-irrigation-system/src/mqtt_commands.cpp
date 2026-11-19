#include "mqtt_commands.h"
#include "config.h"
#include "app_state.h"
#include "logger.h"
#include <ArduinoJson.h>

extern SharedState gState;

// Extracts the device ID segment from "<prefix>/<device_id>/<suffix>".
// Returns false if the topic does not follow that layout.
static bool extractTopicDeviceId(const char *topic, char *out, size_t outSize)
{
    out[0] = '\0';
    const size_t prefixLen = strlen(MQTT_PROJECT_PREFIX);
    if (strncmp(topic, MQTT_PROJECT_PREFIX, prefixLen) != 0 || topic[prefixLen] != '/')
    {
        return false;
    }
    const char *idStart = topic + prefixLen + 1;
    const char *idEnd = strchr(idStart, '/');
    if (idEnd == nullptr || idEnd == idStart)
    {
        return false;
    }
    snprintf(out, outSize, "%.*s", static_cast<int>(idEnd - idStart), idStart);
    return true;
}

// Accepts only "<prefix>/<DEVICE_ID>/command"; logs and rejects everything else.
static bool validateCommandTopic(const char *topic)
{
    char msg[256];
    char topicDeviceId[32];

    if (!extractTopicDeviceId(topic, topicDeviceId, sizeof(topicDeviceId)))
    {
        snprintf(msg, sizeof(msg),
                 "Rejected: malformed topic \"%s\" (expected \"%s/<device_id>/...\")",
                 topic, MQTT_PROJECT_PREFIX);
        logWarn("MQTT", msg);
        return false;
    }

    if (strcmp(topicDeviceId, DEVICE_ID) != 0)
    {
        snprintf(msg, sizeof(msg),
                 "Rejected: message for device \"%s\" on topic \"%s\"; this device is \"%s\"",
                 topicDeviceId, topic, DEVICE_ID);
        logWarn("MQTT", msg);
        return false;
    }

    if (strcmp(topic, MQTT_COMMAND_TOPIC) != 0)
    {
        snprintf(msg, sizeof(msg),
                 "Rejected: unsupported topic \"%s\" (only \"%s\" is accepted)",
                 topic, MQTT_COMMAND_TOPIC);
        logWarn("MQTT", msg);
        return false;
    }

    return true;
}

// Rejects a payload whose optional "device_id" names another device.
static bool payloadDeviceIdMatches(JsonDocument &doc)
{
    if (doc["device_id"].isNull())
    {
        return true;
    }
    const char *payloadDeviceId = doc["device_id"].as<const char *>();
    if (payloadDeviceId != nullptr && strcmp(payloadDeviceId, DEVICE_ID) == 0)
    {
        return true;
    }
    char msg[160];
    snprintf(msg, sizeof(msg),
             "Rejected: payload device_id \"%s\" does not match this device \"%s\"",
             payloadDeviceId ? payloadDeviceId : "<not a string>", DEVICE_ID);
    logWarn("MQTT", msg);
    return false;
}

// ---------------------------------------------------------------------------
// Generic JSON field helpers
// ---------------------------------------------------------------------------

enum class FieldStatus
{
    ABSENT,  // key not present (or null) — caller treats as "not requested"
    VALID,   // key present and matched one of the allowed tokens
    INVALID  // key present but wrong type or unknown token (already logged)
};

// Maps an accepted string token (e.g. "ON") to a typed value.
template <typename T>
struct StringOption
{
    const char *token;
    T value;
};

// Reads doc[key] as a string and maps it to one of `options`.
// Logs a warning and returns INVALID on a wrong type or unknown token.
template <typename T, size_t N>
static FieldStatus parseStringOption(JsonDocument &doc,
                                     const char *key,
                                     const StringOption<T> (&options)[N],
                                     T &out)
{
    JsonVariant field = doc[key];
    if (field.isNull())
    {
        return FieldStatus::ABSENT;
    }

    char msg[128];
    if (!field.is<const char *>())
    {
        snprintf(msg, sizeof(msg), "Rejected: \"%s\" must be a string", key);
        logWarn("MQTT", msg);
        return FieldStatus::INVALID;
    }

    const char *value = field.as<const char *>();
    for (const StringOption<T> &option : options)
    {
        if (strcmp(value, option.token) == 0)
        {
            out = option.value;
            return FieldStatus::VALID;
        }
    }

    // Build "expected A or B or C" from the option table.
    int written = snprintf(msg, sizeof(msg),
                           "Rejected: invalid %s \"%s\"; expected ", key, value);
    for (size_t i = 0; i < N && written > 0 && static_cast<size_t>(written) < sizeof(msg); ++i)
    {
        written += snprintf(msg + written, sizeof(msg) - written, "%s%s",
                            i == 0 ? "" : " or ", options[i].token);
    }
    logWarn("MQTT", msg);
    return FieldStatus::INVALID;
}

// ---------------------------------------------------------------------------
// Command model
// ---------------------------------------------------------------------------

struct MqttCommand
{
    bool hasMode = false;
    OperatingMode mode = OperatingMode::AUTO;

    bool hasPump = false;
    bool pumpOn = false;
};

struct CommandResult
{
    bool modeApplied = false;
    bool pumpApplied = false;
    bool pumpIgnored = false;
};

static const StringOption<OperatingMode> MODE_OPTIONS[] = {
    {"AUTO", OperatingMode::AUTO},
    {"MANUAL", OperatingMode::MANUAL},
};

static const StringOption<bool> PUMP_OPTIONS[] = {
    {"ON", true},
    {"OFF", false},
};

// ---------------------------------------------------------------------------
// Pipeline steps
// ---------------------------------------------------------------------------

// Checks raw MQTT callback arguments and the topic.
static bool validateMessage(const char *topic, const byte *payload, unsigned int length)
{
    constexpr size_t MAX_COMMAND_SIZE = 512;

    if (topic == nullptr || payload == nullptr)
    {
        logWarn("MQTT", "Null topic or payload ignored");
        return false;
    }

    if (!validateCommandTopic(topic))
    {
        return false;
    }

    if (length == 0 || length > MAX_COMMAND_SIZE)
    {
        logWarn("MQTT", "Empty or oversized command ignored");
        return false;
    }

    return true;
}

// Deserializes the payload into `doc`; requires a JSON object.
static bool parseJsonObject(const byte *payload, unsigned int length, JsonDocument &doc)
{
    if (deserializeJson(doc, payload, length))
    {
        logWarn("MQTT", "Invalid JSON command ignored");
        return false;
    }

    if (!doc.is<JsonObject>())
    {
        logWarn("MQTT", "Command must be a JSON object");
        return false;
    }

    return true;
}

// Extracts and validates the supported fields without touching shared state.
static bool parseCommand(JsonDocument &doc, MqttCommand &cmd)
{
    if (!payloadDeviceIdMatches(doc))
    {
        return false;
    }

    const FieldStatus modeStatus = parseStringOption(doc, "mode", MODE_OPTIONS, cmd.mode);
    if (modeStatus == FieldStatus::INVALID)
    {
        return false;
    }

    const FieldStatus pumpStatus = parseStringOption(doc, "pump", PUMP_OPTIONS, cmd.pumpOn);
    if (pumpStatus == FieldStatus::INVALID)
    {
        return false;
    }

    cmd.hasMode = (modeStatus == FieldStatus::VALID);
    cmd.hasPump = (pumpStatus == FieldStatus::VALID);

    if (!cmd.hasMode && !cmd.hasPump)
    {
        logWarn("MQTT", "No supported command found");
        return false;
    }

    return true;
}

// Applies a validated command to shared state under the mutex.
static CommandResult applyCommand(const MqttCommand &cmd)
{
    CommandResult result;

    xSemaphoreTake(gState.mutex, portMAX_DELAY);

    // Any accepted command acknowledges (clears) a latched pump fault.
    gState.pumpFault = PumpFault::NONE;

    // The pump request is evaluated against the mode this command leaves us in.
    const OperatingMode effectiveMode = cmd.hasMode ? cmd.mode : gState.mode;

    if (cmd.hasMode)
    {
        gState.mode = cmd.mode;

        // Clear stale manual requests when entering AUTO.
        if (cmd.mode == OperatingMode::AUTO)
        {
            gState.manualPumpRequested = false;
        }
        result.modeApplied = true;
    }

    if (cmd.hasPump)
    {
        if (effectiveMode == OperatingMode::MANUAL)
        {
            gState.manualPumpRequested = cmd.pumpOn;
            result.pumpApplied = true;
        }
        else
        {
            result.pumpIgnored = true;
        }
    }

    xSemaphoreGive(gState.mutex);
    return result;
}

// Logs the outcome outside the critical section.
static void logCommandResult(const MqttCommand &cmd, const CommandResult &result)
{
    if (result.modeApplied)
    {
        logInfo("MQTT", cmd.mode == OperatingMode::AUTO
                            ? "Mode set to AUTO"
                            : "Mode set to MANUAL");
    }

    if (result.pumpApplied)
    {
        logInfo("MQTT", cmd.pumpOn
                            ? "Manual pump request set to ON"
                            : "Manual pump request set to OFF");
    }

    if (result.pumpIgnored)
    {
        logWarn("MQTT", "Pump command ignored unless mode is MANUAL");
    }
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

void handleMqttCommand(char *topic, byte *payload, unsigned int length)
{
    if (!validateMessage(topic, payload, length))
    {
        return;
    }

    JsonDocument doc;
    if (!parseJsonObject(payload, length, doc))
    {
        return;
    }

    MqttCommand cmd;
    if (!parseCommand(doc, cmd))
    {
        return;
    }

    const CommandResult result = applyCommand(cmd);
    logCommandResult(cmd, result);
}
