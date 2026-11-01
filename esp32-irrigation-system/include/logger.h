#pragma once
#include <Arduino.h>
void loggerBegin();
void logInfo(const char* component, const char* message);
void logWarn(const char* component, const char* message);
void logError(const char* component, const char* message);
