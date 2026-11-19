#pragma once
#include "app_state.h"
#include <Arduino.h>
#include "config.h"

bool buildTelemetryJson(const SensorSnapshot& sensors, OperatingMode mode,
                        bool pumpOn, bool wifi, bool mqtt, char* output, size_t capacity);
