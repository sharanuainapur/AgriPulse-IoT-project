#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

enum class OperatingMode : uint8_t { AUTO = 0, MANUAL = 1 };
enum class PumpFault : uint8_t { NONE = 0, NO_FLOW, MAX_RUNTIME };

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
    bool pumpOn = false;
    bool manualPumpRequested = false;
    PumpFault pumpFault = PumpFault::NONE;
    uint32_t pumpStartedAtMs = 0;
    uint32_t lastFlowPulseAtMs = 0;
};

extern SharedState gState;
