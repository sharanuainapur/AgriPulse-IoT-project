
#pragma once
#include <Arduino.h>

// ESP32 DevKit V1 / ESP32-WROOM-32 pin configuration
// Wi-Fi / MQTT credentials. Never commit real credentials to a public repository.
static constexpr char WIFI_SSID[] = "ACT102692412405_5g";
static constexpr char WIFI_PASSWORD[] = "10487366";
static constexpr char MQTT_HOST[] = "puffin.rmq2.cloudamqp.com";
static constexpr uint16_t MQTT_PORT = 1883;
static constexpr char MQTT_USERNAME[] = "vypyazkf:vypyazkf";
static constexpr char MQTT_PASSWORD[] = "MP0X2SZgC12DNPsb_O_vqM0RK9zJrxpH";
static constexpr char MQTT_CLIENT_ID[] = "nano-esp32-irrigation-01";

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

// 0.96" 128x64 I2C OLED (SSD1306)
static constexpr uint8_t OLED_SDA_PIN = 21;     // GPIO21 (default I2C SDA)
static constexpr uint8_t OLED_SCL_PIN = 22;     // GPIO22 (default I2C SCL)
// Set to the module address if auto-detection does not find the display.
// Common values are 0x3C and 0x3D. Keep 0 for automatic detection.
static constexpr uint8_t OLED_I2C_ADDRESS = 0;
static constexpr uint8_t OLED_WIDTH = 128;
static constexpr uint8_t OLED_HEIGHT = 64;
// Text layout: 4 lines of 21 characters (6x8 font), one line per 16 px band.
// On yellow/blue modules the first line lands in the yellow header band.
static constexpr uint8_t OLED_COLUMNS = OLED_WIDTH / 6;
static constexpr uint8_t OLED_ROWS = 4;
static constexpr uint8_t OLED_CONTRAST = 0x8F;  // 0-255; lower values save power and reduce burn-in
static constexpr uint32_t OLED_REFRESH_INTERVAL_MS = 500UL;
static constexpr uint32_t OLED_PAGE_INTERVAL_MS = 4000UL;

// Relay and LED logic
static constexpr bool PUMP_ACTIVE_LOW = true;
static constexpr bool MOTOR_LED_ACTIVE_LOW = false;

// Sampling / network intervals
static constexpr uint32_t SENSOR_SAMPLE_INTERVAL_MS = 1UL * 5UL * 1000UL;
static constexpr uint32_t MQTT_PUBLISH_INTERVAL_MS = SENSOR_SAMPLE_INTERVAL_MS;
static constexpr uint32_t WIFI_RETRY_INTERVAL_MS = 10000UL;
static constexpr uint32_t MQTT_RETRY_INTERVAL_MS = 5000UL;
static constexpr uint32_t CONTROL_TASK_INTERVAL_MS = 100UL;
static constexpr uint32_t STATUS_LOG_INTERVAL_MS = 10000UL;

// Power saving
// 80 MHz is the lowest CPU clock that still supports Wi-Fi; the default is 240 MHz.
static constexpr uint32_t CPU_FREQUENCY_MHZ = 80;

// Device protection
static constexpr uint32_t POWER_SAFETY_TIMEOUT_MS = 30000UL;
static constexpr uint32_t POWER_STABLE_LOG_INTERVAL_MS = 5000UL;

// Calibrate for your sensors. This assumes higher ADC = drier soil.
static constexpr int SOIL_ADC_DRY = 3505;
static constexpr int SOIL_ADC_WET = 700;
static constexpr float FLOW_PULSES_PER_LITRE = 450.0f; // common YF-S201 starting estimate
static constexpr float SOIL_DRY_THRESHOLD_PERCENT = 30.0f;
static constexpr float SOIL_WET_STOP_PERCENT = 85.0f;
// Raw readings pinned at either ADC rail usually mean a disconnected or shorted probe.
static constexpr int SOIL_ADC_FAULT_LOW = 10;
static constexpr int SOIL_ADC_FAULT_HIGH = 4090;

// Pump protection
static constexpr uint32_t FLOW_START_GRACE_MS = 10000UL;
static constexpr uint32_t NO_FLOW_TIMEOUT_MS = 8000UL;
static constexpr uint32_t MAX_CONTINUOUS_PUMP_MS = 30UL * 60UL * 1000UL;
static constexpr uint16_t MQTT_BUFFER_SIZE = 512;
