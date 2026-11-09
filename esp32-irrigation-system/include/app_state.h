#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

enum class OperatingMode : uint8_t { AUTO = 0, MANUAL = 1 };
enum class PumpFault : uint8_t { NONE = 0, NO_FLOW, MAX_RUNTIME };
// VERIFYING: credentials were just submitted in the portal and are being tried.
enum class WifiSetupStatus : uint8_t { STARTING = 0, CONNECTING, PORTAL, VERIFYING, CONNECTED, ERROR };
// Why the setup portal is open, so the display can tell the user what to do.
enum class PortalReason : uint8_t { NONE = 0, FIRST_SETUP, SAVED_UNREACHABLE, USER_RESET, NEW_WIFI_FAILED };

struct SensorSnapshot {
    int soilAdc = 0;
    float soilMoisturePercent = -1.0f;
    float temperatureC = NAN;
    uint32_t flowPulses = 0;
    float flowLitresPerMinute = 0.0f;
    float totalLitres = 0.0f;
    uint32_t sampledAtMs = 0;
    bool temperatureValid = false;
};

struct SharedState {
    SemaphoreHandle_t mutex = nullptr;
    SensorSnapshot sensors;
    OperatingMode mode = OperatingMode::AUTO;
    bool wifiConnected = false;
    bool mqttConnected = false;
    WifiSetupStatus wifiSetupStatus = WifiSetupStatus::STARTING;
    char wifiSsid[33] = {};
    char wifiPortalSsid[33] = {};
    char wifiPendingSsid[33] = {};   // network being tried from the portal
    char wifiIp[16] = {};
    int8_t wifiRssi = 0;
    int8_t mqttState = -1;           // PubSubClient state code (0 = connected)
    uint8_t portalClients = 0;       // phones joined to the setup access point
    PortalReason portalReason = PortalReason::NONE;
    uint32_t wifiConnectStartedAtMs = 0;
    bool pumpOn = false;
    bool manualPumpRequested = false;
    PumpFault pumpFault = PumpFault::NONE;
    uint32_t pumpStartedAtMs = 0;
    uint32_t lastFlowPulseAtMs = 0;
};

extern SharedState gState;
