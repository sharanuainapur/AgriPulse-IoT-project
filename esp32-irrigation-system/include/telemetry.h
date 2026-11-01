#pragma once
#include "app_state.h"
#include <Arduino.h>

static constexpr char DEVICE_ID[] = "device-01";

bool buildTelemetryJson(const SensorSnapshot& sensors, OperatingMode mode,
                        bool pumpOn, bool wifi, bool mqtt, char* output, size_t capacity);
