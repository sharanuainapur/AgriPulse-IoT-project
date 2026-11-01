#include "logger.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static SemaphoreHandle_t serialMutex = nullptr;
static void writeLog(const char* level, const char* component, const char* message) {
    if (serialMutex) xSemaphoreTake(serialMutex, portMAX_DELAY);
    Serial.printf("[%10lu ms] [%-5s] [%-9s] %s\n",
                  static_cast<unsigned long>(millis()), level, component, message);
    if (serialMutex) xSemaphoreGive(serialMutex);
}
void loggerBegin() { Serial.begin(115200); serialMutex = xSemaphoreCreateMutex(); }
void logInfo(const char* c, const char* m) { writeLog("INFO", c, m); }
void logWarn(const char* c, const char* m) { writeLog("WARN", c, m); }
void logError(const char* c, const char* m) { writeLog("ERROR", c, m); }
