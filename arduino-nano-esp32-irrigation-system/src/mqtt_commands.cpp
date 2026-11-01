#include "mqtt_commands.h"
#include "config.h"
#include "app_state.h"
#include "logger.h"
#include <ArduinoJson.h>

extern SharedState gState;

void handleMqttCommand(char* topic, byte* payload, unsigned int length) {
    String topicString(topic);
    String body;
    body.reserve(length + 1);
    for (unsigned int i = 0; i < length; ++i) body += static_cast<char>(payload[i]);
    Serial.printf("[%10lu ms] [INFO ] [MQTT     ] Command received topic=%s payload=%s\n",
                  static_cast<unsigned long>(millis()), topic, body.c_str());
    if (topicString != MQTT_COMMAND_TOPIC) return;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) { logWarn("MQTT", "Invalid JSON command ignored"); return; }

    bool changed = false;
    if (doc["mode"].is<const char*>()) {
        const char* mode = doc["mode"];
        if (strcmp(mode, "AUTO") == 0) {
            xSemaphoreTake(gState.mutex, portMAX_DELAY);
            gState.mode = OperatingMode::AUTO;
            gState.manualPumpRequested = false;
            xSemaphoreGive(gState.mutex);
            logInfo("MQTT", "Mode set to AUTO");
            changed = true;
        } else if (strcmp(mode, "MANUAL") == 0) {
            xSemaphoreTake(gState.mutex, portMAX_DELAY);
            gState.mode = OperatingMode::MANUAL;
            xSemaphoreGive(gState.mutex);
            logInfo("MQTT", "Mode set to MANUAL");
            changed = true;
        }
    }
    if (doc["pump"].is<bool>()) {
        xSemaphoreTake(gState.mutex, portMAX_DELAY);
        if (gState.mode == OperatingMode::MANUAL) {
            gState.manualPumpRequested = doc["pump"].as<bool>();
            changed = true;
        }
        xSemaphoreGive(gState.mutex);
        if (!changed) logWarn("MQTT", "pump command ignored unless mode is MANUAL");
    }
    if (!changed) logWarn("MQTT", "No supported command applied");
}
