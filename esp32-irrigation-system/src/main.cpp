#include <Arduino.h>
#include <WiFi.h>
#include <esp_system.h>
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
#include "oled_display.h"

SharedState gState;
static SoilSensor soilSensor;
static TemperatureSensor temperatureSensor;
static FlowSensor flowSensor;
static PumpController pump;
static NetworkManager network;
static OledDisplay display;

static void publishSensorSnapshot();
static void SensorTask(void *parameter);
static void NetworkTask(void *parameter);
static void ControlTask(void *parameter);
static void StatusLogTask(void *parameter);
static void DisplayTask(void *parameter);
static void waitForStablePower();

static const char *modeName(OperatingMode mode)
{
    return mode == OperatingMode::AUTO ? "AUTO" : "MANUAL";
}
static void publishSensorSnapshot()
{
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
    if (!buildTelemetryJson(snapshot, mode, pumpOn, wifi, mqtt, payload, sizeof(payload)))
    {
        logError("Telemetry", "Could not serialize JSON");
        return;
    }
    if (network.publish(MQTT_TELEMETRY_TOPIC, payload))
    {
        Serial.printf("[%10lu ms] [INFO ] [Telemetry] Published[%s] %s\n",
                      static_cast<unsigned long>(millis()), MQTT_TELEMETRY_TOPIC, payload);
    }
    else
    {
        logWarn("Telemetry", "Publish skipped/failed (MQTT unavailable)");
    }
}
static void SensorTask(void *)
{
    TickType_t lastWake = xTaskGetTickCount();
    uint32_t totalPulsesPrevious = 0;
    bool first = true;
    for (;;)
    {
        if (first || millis() >= gState.sensors.sampledAtMs + SENSOR_SAMPLE_INTERVAL_MS)
        {
            first = false;
            SensorSnapshot s;
            s.soilAdc = soilSensor.readRaw();
            Serial.printf("Soil sensor read(ADC) - %d\n", s.soilAdc);
            s.soilMoisturePercent = soilSensor.toMoisturePercent(s.soilAdc);
            s.temperatureValid = temperatureSensor.readCelsius(s.temperatureC);
            if (!s.temperatureValid)
                logWarn("SensorTask", "DS18B20 read failed/disconnected");
            const uint32_t pulseCount = flowSensor.readAndResetPulses();
            const uint32_t total = flowSensor.totalPulses();
            const uint32_t elapsedSeconds = (gState.sensors.sampledAtMs == 0)
                                                ? SENSOR_SAMPLE_INTERVAL_MS / 1000
                                                : max<uint32_t>(1, (millis() - gState.sensors.sampledAtMs) / 1000);
            s.flowLitresPerMinute = (static_cast<float>(pulseCount) / FLOW_PULSES_PER_LITRE) * (60.0f / elapsedSeconds);
            s.totalLitres = static_cast<float>(total) / FLOW_PULSES_PER_LITRE;
            s.flowPulses = pulseCount;
            s.sampledAtMs = millis();

            xSemaphoreTake(gState.mutex, portMAX_DELAY);
            gState.sensors = s;
            xSemaphoreGive(gState.mutex);

            Serial.printf("[%10lu ms] [INFO ] [SensorTask] Sample: soil_adc=%d soil=%.1f%% temp=",
                          static_cast<unsigned long>(millis()), s.soilAdc, s.soilMoisturePercent);
            if (s.temperatureValid)
                Serial.printf("%.2f C", s.temperatureC);
            else
                Serial.print("n/a");
            Serial.printf(" flow_pulses=%lu total=%.2f L\n",
                          static_cast<unsigned long>(s.flowPulses), s.totalLitres);
        }
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(1000));
    }
}
// Copies the network status into gState for the display and control tasks.
// Runs on the network task, including from inside WiFiManager while it is
// blocked joining a new network.
static void publishNetworkState()
{
    const bool wifi = network.wifiConnected();
    const bool mqtt = network.mqttConnected();
    const String ssid = network.wifiSsid();
    const String ip = wifi ? WiFi.localIP().toString() : String();
    const int8_t rssi = wifi ? static_cast<int8_t>(WiFi.RSSI()) : 0;
    const uint8_t clients = network.portalClientCount();
    const int8_t mqttState = static_cast<int8_t>(network.mqttState());
    xSemaphoreTake(gState.mutex, portMAX_DELAY);
    gState.wifiConnected = wifi;
    gState.mqttConnected = mqtt;
    gState.wifiSetupStatus = network.wifiSetupStatus();
    gState.portalReason = network.portalReason();
    gState.wifiConnectStartedAtMs = network.connectStartedAtMs();
    gState.wifiRssi = rssi;
    gState.mqttState = mqttState;
    gState.portalClients = clients;
    snprintf(gState.wifiSsid, sizeof(gState.wifiSsid), "%s", ssid.c_str());
    snprintf(gState.wifiPortalSsid, sizeof(gState.wifiPortalSsid), "%s", network.wifiPortalSsid());
    snprintf(gState.wifiPendingSsid, sizeof(gState.wifiPendingSsid), "%s", network.pendingSsid());
    snprintf(gState.wifiIp, sizeof(gState.wifiIp), "%s", ip.c_str());
    xSemaphoreGive(gState.mutex);
}
static void NetworkTask(void *)
{
    TickType_t lastWake = xTaskGetTickCount();
    bool previousWifi = false, previousMqtt = false;
    uint32_t lastPublishMs = 0;
    for (;;)
    {
        network.maintain();
        publishNetworkState();
        const bool wifi = network.wifiConnected();
        const bool mqtt = network.mqttConnected();

        if (wifi != previousWifi)
        {
            logInfo("NetworkTask", wifi ? "Wi-Fi connected" : "Wi-Fi disconnected");
            if (wifi)
                Serial.printf("[%10lu ms] [INFO ] [Network  ] IP address: %s\n",
                              static_cast<unsigned long>(millis()), WiFi.localIP().toString().c_str());
            previousWifi = wifi;
        }
        if (mqtt != previousMqtt)
        {
            logInfo("NetworkTask", mqtt ? "MQTT is online" : "MQTT is offline");
            previousMqtt = mqtt;
            if (mqtt)
            {
                publishSensorSnapshot();
                lastPublishMs = millis();
            }
        }
        if (mqtt && (lastPublishMs == 0 || millis() - lastPublishMs >= MQTT_PUBLISH_INTERVAL_MS))
        {
            publishSensorSnapshot();
            lastPublishMs = millis();
        }
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(50));
    }
}
static void ControlTask(void *)
{
    TickType_t lastWake = xTaskGetTickCount();
    PumpFault fault;
    bool localPump = false;
    uint32_t pumpStartedAt = 0;
    uint32_t lastPulseAt = 0;
    for (;;)
    {
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
        fault = gState.pumpFault;
        xSemaphoreGive(gState.mutex);

        // AUTO is allowed only when BOTH Wi-Fi and MQTT are connected.
        // If either is unavailable, fall back to local soil-based control.
        bool desiredPump = false;
        if (mode == OperatingMode::MANUAL && wifi && mqtt)
        {
            desiredPump = manualRequest;
        }
        else if ((mode == OperatingMode::AUTO && wifi && mqtt) || !wifi || !mqtt)
        {
            if (sensors.soilMoisturePercent >= 0)
            {
                if (!localPump && sensors.soilMoisturePercent <= SOIL_DRY_THRESHOLD_PERCENT)
                    desiredPump = true;
                else if (localPump && sensors.soilMoisturePercent < SOIL_WET_STOP_PERCENT)
                    desiredPump = true;
            }
        }
        else
        {
            desiredPump = false;
        }

        // Pump safety interlocks always override the desired state.
        if (fault != PumpFault::NONE)
        {
            // logError("Safety", " pump fault");
            desiredPump = false;
        }
        if (!localPump && desiredPump)
        {
            localPump = true;
            pumpStartedAt = now;
            lastPulseAt = flowSensor.lastPulseAtMs();
            xSemaphoreTake(gState.mutex, portMAX_DELAY);
            gState.pumpStartedAtMs = now;
            xSemaphoreGive(gState.mutex);
            pump.setPump(true, "control policy requested watering");
        }
        if (localPump)
        {
            const uint32_t lastSeenPulse = flowSensor.lastPulseAtMs();
            if (lastSeenPulse != 0)
                lastPulseAt = lastSeenPulse;
            if (now - pumpStartedAt > MAX_CONTINUOUS_PUMP_MS)
            {
                xSemaphoreTake(gState.mutex, portMAX_DELAY);
                gState.pumpFault = PumpFault::MAX_RUNTIME;
                xSemaphoreGive(gState.mutex);
                logError("Safety", "Maximum continuous pump runtime exceeded; pump latched OFF until reboot");
                desiredPump = false;
            }
            else if (now - pumpStartedAt > FLOW_START_GRACE_MS &&
                     (lastPulseAt == 0 || now - lastPulseAt > NO_FLOW_TIMEOUT_MS))
            {
                xSemaphoreTake(gState.mutex, portMAX_DELAY);
                gState.pumpFault = PumpFault::NO_FLOW;
                xSemaphoreGive(gState.mutex);
                logError("Safety", "No water flow detected; pump latched OFF until reboot");
                desiredPump = false;
            }
        }
        if (localPump && !desiredPump)
        {
            localPump = false;
            // xSemaphoreTake(gState.mutex, portMAX_DELAY);
            // gState.pumpFault = PumpFault::NONE;
            // xSemaphoreGive(gState.mutex);
            pump.setPump(false, "control policy or safety interlock");
        }

        xSemaphoreTake(gState.mutex, portMAX_DELAY);
        gState.pumpOn = localPump;
        gState.lastFlowPulseAtMs = lastPulseAt;
        xSemaphoreGive(gState.mutex);
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(CONTROL_TASK_INTERVAL_MS));
    }
}
static void StatusLogTask(void *)
{
    TickType_t lastWake = xTaskGetTickCount();
    for (;;)
    {
        xSemaphoreTake(gState.mutex, portMAX_DELAY);
        Serial.printf("[%10lu ms] [INFO ] [Status   ] mode=%s wifi=%s mqtt=%s pump=%s soil=%.1f%% temp=",
                      static_cast<unsigned long>(millis()), modeName(gState.mode),
                      gState.wifiConnected ? "up" : "down", gState.mqttConnected ? "up" : "down",
                      gState.pumpOn ? "ON" : "OFF", gState.sensors.soilMoisturePercent);
        if (gState.sensors.temperatureValid)
            Serial.printf("%.2f C", gState.sensors.temperatureC);
        else
            Serial.print("n/a");
        Serial.printf(" fault=%d\n", static_cast<int>(gState.pumpFault));
        xSemaphoreGive(gState.mutex);
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(STATUS_LOG_INTERVAL_MS));
    }
}
// Only the owner of the OLED after boot; setup() writes to it before this task exists.
static void DisplayTask(void *)
{
    TickType_t lastWake = xTaskGetTickCount();
    for (;;)
    {
        DisplayStatus status;
        xSemaphoreTake(gState.mutex, portMAX_DELAY);
        status.sensors = gState.sensors;
        status.mode = gState.mode;
        status.wifiConnected = gState.wifiConnected;
        status.mqttConnected = gState.mqttConnected;
        status.wifiSetupStatus = gState.wifiSetupStatus;
        status.portalReason = gState.portalReason;
        status.wifiConnectStartedAtMs = gState.wifiConnectStartedAtMs;
        status.wifiRssi = gState.wifiRssi;
        status.mqttState = gState.mqttState;
        status.portalClients = gState.portalClients;
        snprintf(status.wifiSsid, sizeof(status.wifiSsid), "%s", gState.wifiSsid);
        snprintf(status.wifiPortalSsid, sizeof(status.wifiPortalSsid), "%s", gState.wifiPortalSsid);
        snprintf(status.wifiPendingSsid, sizeof(status.wifiPendingSsid), "%s", gState.wifiPendingSsid);
        snprintf(status.wifiIp, sizeof(status.wifiIp), "%s", gState.wifiIp);
        status.pumpOn = gState.pumpOn;
        status.pumpFault = gState.pumpFault;
        status.pumpStartedAtMs = gState.pumpStartedAtMs;
        xSemaphoreGive(gState.mutex);
        display.showDashboard(status);
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(OLED_REFRESH_INTERVAL_MS));
    }
}
// Second OLED line during boot: warns the user when the last restart was abnormal.
static const char *resetDisplayText(esp_reset_reason_t reason)
{
    switch (reason)
    {
    case ESP_RST_BROWNOUT:
        return "!Last: power dip";
    case ESP_RST_PANIC:
        return "!Last: crash";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:
        return "!Last: watchdog";
    default:
        return "Smart Irrigation";
    }
}
static const char *resetReasonName(esp_reset_reason_t reason)
{
    switch (reason)
    {
    case ESP_RST_POWERON:
        return "power-on";
    case ESP_RST_BROWNOUT:
        return "brownout (supply voltage dropped)";
    case ESP_RST_SW:
        return "software restart";
    case ESP_RST_PANIC:
        return "crash/panic";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:
        return "watchdog";
    case ESP_RST_EXT:
        return "external reset pin";
    case ESP_RST_DEEPSLEEP:
        return "deep sleep wake";
    default:
        return "unknown";
    }
}

// Hold all operations until power has been stable for POWER_SAFETY_TIMEOUT_MS.
// A supply dip during this window triggers the ESP32 brownout detector, which
// resets the chip, so the full window restarts from zero after every dip.
static void waitForStablePower()
{
    char msg[96];
    const esp_reset_reason_t reason = esp_reset_reason();
    snprintf(msg, sizeof(msg), "Reset reason: %s", resetReasonName(reason));
    if (reason == ESP_RST_BROWNOUT)
        logWarn("Power", msg);
    else
        logInfo("Power", msg);
    display.showLine(1, resetDisplayText(reason));
    display.showLine(2, "Power check...");

    const uint32_t windowSeconds = POWER_SAFETY_TIMEOUT_MS / 1000UL;
    snprintf(msg, sizeof(msg), "Waiting for %lu s of stable power before starting sensors",
             static_cast<unsigned long>(windowSeconds));
    logInfo("Power", msg);

    const uint32_t startMs = millis();
    uint32_t lastReportMs = startMs;
    uint32_t lastDisplayMs = 0;
    while (millis() - startMs < POWER_SAFETY_TIMEOUT_MS)
    {
        if (lastDisplayMs == 0 || millis() - lastDisplayMs >= 1000UL)
        {
            lastDisplayMs = millis();
            snprintf(msg, sizeof(msg), "Stable %2lu/%lu s",
                     static_cast<unsigned long>((lastDisplayMs - startMs) / 1000UL),
                     static_cast<unsigned long>(windowSeconds));
            display.showLine(3, msg);
        }
        if (millis() - lastReportMs >= POWER_STABLE_LOG_INTERVAL_MS)
        {
            lastReportMs = millis();
            snprintf(msg, sizeof(msg), "Power stable for %lu/%lu s",
                     static_cast<unsigned long>((lastReportMs - startMs) / 1000UL),
                     static_cast<unsigned long>(windowSeconds));
            logInfo("Power", msg);
        }
        delay(100);
    }
    logInfo("Power", "Power stable; starting operations");
}

void setup()
{
    // Lower the CPU clock first; the APB bus stays at 80 MHz, so UART/ADC timing is unchanged.
    setCpuFrequencyMhz(CPU_FREQUENCY_MHZ);
    loggerBegin();
    // Drive the relay to OFF before waiting, so the pump pin never floats.
    pump.begin();
    display.begin();
    display.showMessage("   AgriPulse", "Smart Irrigation", "Starting...", "Pump: OFF (safe)");
    waitForStablePower();
    display.showMessage("   AgriPulse", "Power OK", "Init sensors...");
    logInfo("Boot", "ESP32 DevKit irrigation system starting");
    Serial.printf("[%10lu ms] [INFO ] [Boot     ] CPU frequency: %lu MHz\n",
                  static_cast<unsigned long>(millis()), static_cast<unsigned long>(getCpuFrequencyMhz()));
    pinMode(POWER_LED_PIN, OUTPUT);
    digitalWrite(POWER_LED_PIN, HIGH);
    gState.mutex = xSemaphoreCreateMutex();
    if (!gState.mutex)
    {
        logError("Boot", "Failed to create state mutex; restarting");
        display.showMessage("!! BOOT ERROR !!", "Out of memory", "Restarting...");
        delay(1000);
        ESP.restart();
    }
    soilSensor.begin();
    temperatureSensor.begin();
    flowSensor.begin();
    network.setCommandCallback(handleMqttCommand);
    network.setStatusListener(publishNetworkState);
    network.begin();
    display.showMessage("   AgriPulse", "Sensors ready", "WiFi setup starting");

    logInfo("Boot", "Creating FreeRTOS tasks");
    xTaskCreatePinnedToCore(SensorTask, "SensorTask", 4096, nullptr, 2, nullptr, 1);
    xTaskCreatePinnedToCore(NetworkTask, "NetworkTask", 12288, nullptr, 2, nullptr, 0);
    xTaskCreatePinnedToCore(ControlTask, "ControlTask", 4096, nullptr, 3, nullptr, 1);
    xTaskCreatePinnedToCore(StatusLogTask, "StatusLogTask", 4096, nullptr, 1, nullptr, 0);
    if (display.available())
        xTaskCreatePinnedToCore(DisplayTask, "DisplayTask", 4096, nullptr, 1, nullptr, 0);
    logInfo("Boot", "Startup complete");
}
void loop() { vTaskDelay(pdMS_TO_TICKS(1000)); }
