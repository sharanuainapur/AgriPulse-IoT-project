
#pragma once
#include <Arduino.h>

// ESP32 DevKit V1 / ESP32-WROOM-32 pin configuration
// Wi-Fi / MQTT credentials. Never commit real credentials to a public repository.
static constexpr char WIFI_SSID[] = "<>";
static constexpr char WIFI_PASSWORD[] = "<>";
static constexpr char MQTT_HOST[] = "<>";
static constexpr uint16_t MQTT_PORT = 1883;
static constexpr char MQTT_USERNAME[] = "<>";
static constexpr char MQTT_PASSWORD[] = "<>";
static constexpr char MQTT_CLIENT_ID[] = "<>";

static constexpr char MQTT_TELEMETRY_TOPIC[] = "agripulse/device-01/telemetry";
static constexpr char MQTT_COMMAND_TOPIC[]   = "agripulse/device-01/command";
static constexpr char MQTT_STATUS_TOPIC[]    = "agripulse/device-01/status";
static constexpr char MQTT_MOTOR_STATE_TOPIC[] = "agripulse/device-01/mstatus";

// Analog soil moisture sensor
static constexpr uint8_t SOIL_SENSOR_PIN = 34;  // GPIO34 (ADC1, input only)

// DS18B20 temperature sensor
static constexpr uint8_t TEMPERATURE_PIN = 5;   // GPIO5, 4.7k pull-up to 3.3V

// Water flow meter pulse output
static constexpr uint8_t FLOW_SENSOR_PIN = 27;  // GPIO27

// Pump relay / MOSFET driver
static constexpr uint8_t PUMP_RELAY_PIN = 26;   // GPIO26

// Motor ON/OFF indicator LED
static constexpr uint8_t MOTOR_LED_PIN = 25;    // GPIO25

// Device power indicator LED
static constexpr uint8_t POWER_LED_PIN = 2;     // GPIO2 (onboard LED on many DevKit boards)

// Relay and LED logic
static constexpr bool PUMP_ACTIVE_LOW = false;
static constexpr bool MOTOR_LED_ACTIVE_LOW = false;

// Sampling / network intervals
static constexpr uint32_t SENSOR_SAMPLE_INTERVAL_MS = 1UL * 5UL * 1000UL;
static constexpr uint32_t MQTT_PUBLISH_INTERVAL_MS = SENSOR_SAMPLE_INTERVAL_MS;
static constexpr uint32_t WIFI_RETRY_INTERVAL_MS = 10000UL;
static constexpr uint32_t MQTT_RETRY_INTERVAL_MS = 5000UL;
static constexpr uint32_t CONTROL_TASK_INTERVAL_MS = 100UL;
static constexpr uint32_t STATUS_LOG_INTERVAL_MS = 10000UL;

// Device protection
static constexpr uint32_t POWER_SAFETY_TIMEOUT_MS = 30000UL;

// Calibrate for your sensors. This assumes higher ADC = drier soil.
static constexpr int SOIL_ADC_DRY = 3505;
static constexpr int SOIL_ADC_WET = 700;
static constexpr float FLOW_PULSES_PER_LITRE = 450.0f; // common YF-S201 starting estimate
static constexpr float SOIL_DRY_THRESHOLD_PERCENT = 30.0f;
static constexpr float SOIL_WET_STOP_PERCENT = 85.0f;

// Pump protection
static constexpr uint32_t FLOW_START_GRACE_MS = 10000UL;
static constexpr uint32_t NO_FLOW_TIMEOUT_MS = 8000UL;
static constexpr uint32_t MAX_CONTINUOUS_PUMP_MS = 30UL * 60UL * 1000UL;
static constexpr uint16_t MQTT_BUFFER_SIZE = 512;
