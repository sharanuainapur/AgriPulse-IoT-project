#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "app_state.h"
#include "logger.h"
#include "soil_sensor.h"
#include "temperature_sensor.h"
#include "flow_sensor.h"
#include "pump_controller.h"
#include "network_manager.h"
#include "mqtt_commands.h"
#include "telemetry.h"

SharedState gState;
static SoilSensor soilSensor;
static TemperatureSensor temperatureSensor;
static FlowSensor flowSensor;
static PumpController pump;
static NetworkManager network;

static void publishSensorSnapshot();
static void SensorTask(void* parameter);
static void NetworkTask(void* parameter);
static void ControlTask(void* parameter);
static void StatusLogTask(void* parameter);

static const char* modeName(OperatingMode mode) {
    return mode == OperatingMode::AUTO ? "AUTO" : "MANUAL";
}
static void publishSensorSnapshot() {
    SensorSnapshot snapshot;
    OperatingMode mode;
    bool pumpOn, wifi, mqtt;
    xSemaphoreTake(gState.mutex, portMAX_DELAY);
    snapshot = gState.sensors;
    mode = gState.mode;
    pumpOn = gState.pumpOn;
    wifi = gState.wifiConnected;
    mqtt = gState.mqttConnected;
    xSemaphoreGive(gState.mutex);

    char payload[MQTT_BUFFER_SIZE];
    if (!buildTelemetryJson(snapshot, mode, pumpOn, wifi, mqtt, payload, sizeof(payload))) {
        logError("Telemetry", "Could not serialize JSON");
        return;
    }
    if (network.publish(MQTT_TELEMETRY_TOPIC, payload)) {
        Serial.printf("[%10lu ms] [INFO ] [Telemetry] Published %s\n",
                      static_cast<unsigned long>(millis()), payload);
    } else {
        logWarn("Telemetry", "Publish skipped/failed (MQTT unavailable)");
    }
}
static void SensorTask(void*) {
    TickType_t lastWake = xTaskGetTickCount();
    uint32_t totalPulsesPrevious = 0;
    bool first = true;
    for (;;) {
        if (first || millis() >= gState.sensors.sampledAtMs + SENSOR_SAMPLE_INTERVAL_MS) {
            first = false;
            SensorSnapshot s;
            s.soilAdc = soilSensor.readRaw();
            Serial.printf("Soil sensor read - %d", s.soilAdc);
            s.soilMoisturePercent = soilSensor.toMoisturePercent(s.soilAdc);
            s.temperatureValid = temperatureSensor.readCelsius(s.temperatureC);
            if (!s.temperatureValid) logWarn("SensorTask", "DS18B20 read failed/disconnected");
            const uint32_t pulseCount = flowSensor.readAndResetPulses();
            const uint32_t total = flowSensor.totalPulses();
            const uint32_t elapsedSeconds = (gState.sensors.sampledAtMs == 0)
                ? SENSOR_SAMPLE_INTERVAL_MS / 1000
                : max<uint32_t>(1, (millis() - gState.sensors.sampledAtMs) / 1000);
            s.flowLitresPerMinute = (static_cast<float>(pulseCount) / FLOW_PULSES_PER_LITRE)
                                    * (60.0f / elapsedSeconds);
            s.totalLitres = static_cast<float>(total) / FLOW_PULSES_PER_LITRE;
            s.flowPulses = pulseCount;
            s.sampledAtMs = millis();

            xSemaphoreTake(gState.mutex, portMAX_DELAY);
            gState.sensors = s;
            xSemaphoreGive(gState.mutex);

            Serial.printf("[%10lu ms] [INFO ] [SensorTask] Sample: soil_adc=%d soil=%.1f%% temp=",
                          static_cast<unsigned long>(millis()), s.soilAdc, s.soilMoisturePercent);
            if (s.temperatureValid) Serial.printf("%.2f C", s.temperatureC);
            else Serial.print("n/a");
            Serial.printf(" flow_pulses=%lu total=%.2f L\n",
                          static_cast<unsigned long>(s.flowPulses), s.totalLitres);
        }
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(1000));
    }
}
static void NetworkTask(void*) {
    TickType_t lastWake = xTaskGetTickCount();
    bool previousWifi = false, previousMqtt = false;
    uint32_t lastPublishMs = 0;
    for (;;) {
        network.maintain();
        const bool wifi = network.wifiConnected();
        const bool mqtt = network.mqttConnected();
        xSemaphoreTake(gState.mutex, portMAX_DELAY);
        gState.wifiConnected = wifi;
        gState.mqttConnected = mqtt;
        xSemaphoreGive(gState.mutex);

        if (wifi != previousWifi) {
            logInfo("NetworkTask", wifi ? "Wi-Fi connected" : "Wi-Fi disconnected");
            if (wifi) Serial.printf("[%10lu ms] [INFO ] [Network  ] IP address: %s\n",
                                     static_cast<unsigned long>(millis()), WiFi.localIP().toString().c_str());
            previousWifi = wifi;
        }
        if (mqtt != previousMqtt) {
            logInfo("NetworkTask", mqtt ? "MQTT is online" : "MQTT is offline");
            previousMqtt = mqtt;
            if (mqtt) { publishSensorSnapshot(); lastPublishMs = millis(); }
        }
        if (mqtt && (lastPublishMs == 0 || millis() - lastPublishMs >= MQTT_PUBLISH_INTERVAL_MS)) {
            publishSensorSnapshot();
            lastPublishMs = millis();
        }
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(50));
    }
}
static void ControlTask(void*) {
    TickType_t lastWake = xTaskGetTickCount();
    bool localPump = false;
    uint32_t pumpStartedAt = 0;
    uint32_t lastPulseAt = 0;
    for (;;) {
        const uint32_t now = millis();
        SensorSnapshot sensors;
        OperatingMode mode;
        bool wifi, mqtt, manualRequest;
        xSemaphoreTake(gState.mutex, portMAX_DELAY);
        sensors = gState.sensors;
        mode = gState.mode;
        wifi = gState.wifiConnected;
        mqtt = gState.mqttConnected;
        manualRequest = gState.manualPumpRequested;
        xSemaphoreGive(gState.mutex);

        // AUTO is allowed only when BOTH Wi-Fi and MQTT are connected.
        // If either is unavailable, fall back to local soil-based control.
        bool desiredPump = false;
        if (mode == OperatingMode::MANUAL && wifi && mqtt) {
            desiredPump = manualRequest;
        } else if ((mode == OperatingMode::AUTO && wifi && mqtt) || !wifi || !mqtt) {
            if (sensors.soilMoisturePercent >= 0) {
                if (!localPump && sensors.soilMoisturePercent <= SOIL_DRY_THRESHOLD_PERCENT)
                    desiredPump = true;
                else if (localPump && sensors.soilMoisturePercent < SOIL_WET_STOP_PERCENT)
                    desiredPump = true;
            }
        } else {
            desiredPump = false;
        }

        // Pump safety interlocks always override the desired state.
        if (gState.pumpFault != PumpFault::NONE) desiredPump = false;
        if (!localPump && desiredPump) {
            localPump = true;
            pumpStartedAt = now;
            lastPulseAt = flowSensor.lastPulseAtMs();
            xSemaphoreTake(gState.mutex, portMAX_DELAY);
            gState.pumpStartedAtMs = now;
            xSemaphoreGive(gState.mutex);
            pump.setPump(true, "control policy requested watering");
        }
        if (localPump) {
            const uint32_t lastSeenPulse = flowSensor.lastPulseAtMs();
            if (lastSeenPulse != 0) lastPulseAt = lastSeenPulse;
            if (now - pumpStartedAt > MAX_CONTINUOUS_PUMP_MS) {
                xSemaphoreTake(gState.mutex, portMAX_DELAY);
                gState.pumpFault = PumpFault::MAX_RUNTIME;
                xSemaphoreGive(gState.mutex);
                logError("Safety", "Maximum continuous pump runtime exceeded; pump latched OFF until reboot");
                desiredPump = false;
            } else if (now - pumpStartedAt > FLOW_START_GRACE_MS &&
                       (lastPulseAt == 0 || now - lastPulseAt > NO_FLOW_TIMEOUT_MS)) {
                xSemaphoreTake(gState.mutex, portMAX_DELAY);
                gState.pumpFault = PumpFault::NO_FLOW;
                xSemaphoreGive(gState.mutex);
                logError("Safety", "No water flow detected; pump latched OFF until reboot");
                desiredPump = false;
            }
        }
        if (localPump && !desiredPump) {
            localPump = false;
            pump.setPump(false, "control policy or safety interlock");
        }

        xSemaphoreTake(gState.mutex, portMAX_DELAY);
        gState.pumpOn = localPump;
        gState.lastFlowPulseAtMs = lastPulseAt;
        xSemaphoreGive(gState.mutex);
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(CONTROL_TASK_INTERVAL_MS));
    }
}
static void StatusLogTask(void*) {
    TickType_t lastWake = xTaskGetTickCount();
    for (;;) {
        xSemaphoreTake(gState.mutex, portMAX_DELAY);
        Serial.printf("[%10lu ms] [INFO ] [Status   ] mode=%s wifi=%s mqtt=%s pump=%s soil=%.1f%% temp=",
            static_cast<unsigned long>(millis()), modeName(gState.mode),
            gState.wifiConnected ? "up" : "down", gState.mqttConnected ? "up" : "down",
            gState.pumpOn ? "ON" : "OFF", gState.sensors.soilMoisturePercent);
        if (gState.sensors.temperatureValid) Serial.printf("%.2f C", gState.sensors.temperatureC);
        else Serial.print("n/a");
        Serial.printf(" fault=%d\n", static_cast<int>(gState.pumpFault));
        xSemaphoreGive(gState.mutex);
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(STATUS_LOG_INTERVAL_MS));
    }
}
void setup() {
    loggerBegin();
    delay(300);
    logInfo("Boot", "Arduino Nano ESP32 irrigation system starting");
    pinMode(POWER_LED_PIN, OUTPUT);
    digitalWrite(POWER_LED_PIN, HIGH);
    gState.mutex = xSemaphoreCreateMutex();
    if (!gState.mutex) {
        logError("Boot", "Failed to create state mutex; restarting");
        delay(1000);
        ESP.restart();
    }
    Serial.print("Sensors found: ");
    soilSensor.begin();
    Serial.print("Sensors found: ");
    temperatureSensor.begin();
    flowSensor.begin();
    pump.begin();
    network.setCommandCallback(handleMqttCommand);
    network.begin();

    logInfo("Boot", "Creating FreeRTOS tasks");
    xTaskCreatePinnedToCore(SensorTask, "SensorTask", 4096, nullptr, 2, nullptr, 1);
    xTaskCreatePinnedToCore(NetworkTask, "NetworkTask", 6144, nullptr, 2, nullptr, 0);
    xTaskCreatePinnedToCore(ControlTask, "ControlTask", 4096, nullptr, 3, nullptr, 1);
    xTaskCreatePinnedToCore(StatusLogTask, "StatusLogTask", 4096, nullptr, 1, nullptr, 0);
    logInfo("Boot", "Startup complete");
}
void loop() { vTaskDelay(pdMS_TO_TICKS(1000)); }
