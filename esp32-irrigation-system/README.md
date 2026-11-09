# esp32-irrigation-system

Modular PlatformIO starter project for the ESP32 DevKit (ESP32-WROOM-32) using Arduino framework and FreeRTOS tasks.

## Features
- Separate modules for soil, temperature, flow, pump, logging, networking, MQTT commands, and telemetry.
- FreeRTOS tasks for sensor sampling, network/MQTT, pump control, and status logging.
- Sensor sample every 5 minutes (first sample is immediate).
- MQTT telemetry and command subscription.
- Local soil-based fallback when Wi-Fi or MQTT is unavailable.
- Pump safety: stop and latch pump OFF when flow is not detected, and enforce a maximum continuous run time.
- Timestamped serial logs at 115200 baud.
- Power stabilization: after boot the pump is held OFF and nothing else starts until power has been stable for `POWER_SAFETY_TIMEOUT_MS` (30 s). A supply dip triggers a brownout reset, which restarts the wait.

## Hardware assumptions
- Soil sensor analog output -> `GPIO34` (ADC1, input only).
- DS18B20 temperature sensor data -> `GPIO5`, with a 4.7k pull-up between data and 3.3V.
- Flow sensor pulse output -> `GPIO27`. Ensure the output voltage is safe for the ESP32 3.3V GPIO.
- Pump relay/driver input -> `GPIO26` (active-LOW by default, see `PUMP_ACTIVE_LOW`).
- Motor status LED -> `GPIO25` through a suitable resistor.
- Power LED -> `GPIO2` (onboard LED on many DevKit boards).
- 0.96" 128x64 I2C OLED (SSD1306) -> SDA `GPIO21`, SCL `GPIO22`, VCC 3.3V, GND. The I2C address (`0x3C` or `0x3D`) is auto-detected. Power the module from 3.3V so its on-board pull-ups keep SDA/SCL at ESP32-safe levels. The system runs normally without the OLED.
- Use a proper relay/MOSFET driver and a separate appropriately rated power supply for the pump. **Never drive a pump directly from an ESP32 GPIO.** Add flyback protection for inductive DC loads where appropriate.

Confirm pin labels and electrical limits for your exact ESP32 DevKit board before wiring.

## Configure
Edit `include/config.h`:
1. Set `WIFI_SSID` and `WIFI_PASSWORD`.
2. Set `MQTT_HOST`, `MQTT_PORT`, and optional credentials.
3. Set sensor and output pins if your wiring differs.
4. If the OLED is not auto-detected, set `OLED_I2C_ADDRESS` to its scanned address (usually `0x3C` or `0x3D`). Lower `OLED_CONTRAST` to reduce power draw and burn-in.
5. Calibrate `SOIL_ADC_DRY` and `SOIL_ADC_WET` from your sensor readings.
6. Calibrate `FLOW_PULSES_PER_LITRE` for your flow sensor.
7. Adjust watering thresholds and pump safety timeouts.

Install PlatformIO, open this directory, then build/upload:
```bash
pio run
pio run --target upload
pio device monitor --baud 115200
```

## MQTT topics
- Telemetry: `irrigation/device-01/telemetry`
- Commands: `irrigation/device-01/command`
- Status: `irrigation/device-01/status`

Example commands (publish JSON):
```bash
mosquitto_pub -h 192.168.1.100 -t irrigation/device-01/command -m '{"mode":"MANUAL","pump":true}'
mosquitto_pub -h 192.168.1.100 -t irrigation/device-01/command -m '{"mode":"MANUAL","pump":false}'
mosquitto_pub -h 192.168.1.100 -t irrigation/device-01/command -m '{"mode":"AUTO"}'
```

## Mode behaviour
- `AUTO` with Wi-Fi **and** MQTT connected: the device accepts the configured automatic soil thresholds and publishes telemetry/subscribes to commands.
- `MANUAL` with network connected: pump follows the MQTT `pump` boolean command. If connectivity fails, the controller falls back to local soil-based watering so the system can continue autonomously.
- If Wi-Fi or MQTT is unavailable, local soil-based watering is enabled regardless of the last mode; this follows the requested offline fallback behaviour.
- The pump is always stopped on a no-flow fault or maximum runtime fault. Faults are latched until reboot, so inspect plumbing/sensor wiring before restarting.

## Sensor timing notes
Sensor values are sampled every 5 minutes. Flow pulses are counted continuously by an interrupt so pump protection does not wait for the next sensor sample. `water_flow_lpm` in telemetry is an estimate over the latest sampling interval; for accurate real-time flow-rate reporting, add a separate short-interval flow-rate calculation.

## Important notes
- This is a reusable starter project and should be built with your installed PlatformIO toolchain and tested with the actual sensors/relay before connecting the pump.
- Sensor ADC calibration, relay polarity, flow pulse calibration, and exact pin compatibility depend on your hardware.
- MQTT uses a non-TLS `WiFiClient` on port 1883 by default. Use a trusted private network or add TLS/authentication for production deployments.
