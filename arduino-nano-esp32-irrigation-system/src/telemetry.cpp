#include "telemetry.h"
#include "config.h"
#include <ArduinoJson.h>
bool buildTelemetryJson(const SensorSnapshot& s, OperatingMode mode, bool pumpOn,
                        bool wifi, bool mqtt, char* output, size_t capacity) {
    JsonDocument doc;
    doc["device_id"] = DEVICE_ID;
    doc["mode"] = (mode == OperatingMode::AUTO) ? "AUTO" : "MANUAL";
    // doc["soil_adc"] = s.soilAdc;
    if (s.soilMoisturePercent >= 0) doc["soil_moisture_pct"] = s.soilMoisturePercent;
    else doc["soil_moisture_pct"] = nullptr;
    if (s.temperatureValid) doc["temperature_c"] = s.temperatureC;
    else doc["temperature_c"] = nullptr;
    doc["water_flow_lpm"] = s.flowLitresPerMinute;
    doc["total_litres"] = s.totalLitres;
    doc["motor_state"] = pumpOn ? "ON" : "OFF";
    doc["wifi_connected"] = wifi;
    doc["mqtt_connected"] = mqtt;
    doc["recorded_at"] = s.sampledAtMs;
    return serializeJson(doc, output, capacity) > 0;
}
