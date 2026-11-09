#include "oled_display.h"
#include "logger.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

static Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire, -1); // -1: module has no reset pin

static constexpr uint8_t LINE_HEIGHT_PX = OLED_HEIGHT / OLED_ROWS; // 16 px per text line
static constexpr uint8_t TEXT_OFFSET_PX = (LINE_HEIGHT_PX - 8) / 2; // centre the 8 px font
static constexpr uint8_t MAX_ALERTS = OLED_ROWS - 1; // first row is the alert title
static const char DEGREE[] = "\xF8";                 // degree sign in the CP437 font

static bool soilSensorFault(const SensorSnapshot& s)
{
    return s.soilAdc <= SOIL_ADC_FAULT_LOW || s.soilAdc >= SOIL_ADC_FAULT_HIGH;
}

// Fills alerts[] with one user-facing line per active problem, most serious first.
static uint8_t collectAlerts(const DisplayStatus& s, char alerts[][OLED_COLUMNS + 1])
{
    const char* found[6];
    uint8_t count = 0;
    if (s.pumpFault == PumpFault::NO_FLOW)
        found[count++] = "PUMP OFF:NO FLOW";
    else if (s.pumpFault == PumpFault::MAX_RUNTIME)
        found[count++] = "PUMP OFF:MAX RUN";
    if (s.sensors.sampledAtMs != 0)
    {
        if (!s.sensors.temperatureValid)
            found[count++] = "Temp sensor fail";
        if (soilSensorFault(s.sensors))
            found[count++] = "Soil sensor fail";
    }
    if (!s.wifiConnected)
        found[count++] = "No WiFi: local";
    else if (!s.mqttConnected)
        found[count++] = "No MQTT: local";
    if (s.pumpFault != PumpFault::NONE)
        found[count++] = "Reset: cmd/boot";

    for (uint8_t i = 0; i < count && i < MAX_ALERTS; ++i)
        snprintf(alerts[i], OLED_COLUMNS + 1, "%s", found[i]);
    return count;
}

static bool i2cDevicePresent(uint8_t address)
{
    Wire.beginTransmission(address);
    return Wire.endTransmission() == 0;
}

bool OledDisplay::begin()
{
    Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
    Wire.setClock(400000);
    uint8_t devices = 0;
    for (uint8_t address = 1; address < 127; ++address)
    {
        if (i2cDevicePresent(address))
        {
            ++devices;
            char msg[48];
            snprintf(msg, sizeof(msg), "I2C device found at 0x%02X", address);
            logInfo("OLED", msg);
        }
    }
    if (devices == 0)
        logWarn("OLED", "No I2C devices found; check VCC, GND, SDA=GPIO21, SCL=GPIO22");

    uint8_t address = OLED_I2C_ADDRESS;
    if (address == 0)
        address = i2cDevicePresent(0x3C) ? 0x3C : i2cDevicePresent(0x3D) ? 0x3D : 0;
    if (address == 0 || !i2cDevicePresent(address))
    {
        logWarn("OLED", "SSD1306 not found at 0x3C/0x3D; set OLED_I2C_ADDRESS if it uses another address");
        return false;
    }
    // Adafruit_SSD1306::begin() only fails when the 1 KB frame buffer cannot be allocated.
    if (!oled.begin(SSD1306_SWITCHCAPVCC, address))
    {
        logWarn("OLED", "OLED init failed (out of memory)");
        return false;
    }
    oled.ssd1306_command(SSD1306_SETCONTRAST);
    oled.ssd1306_command(OLED_CONTRAST);
    oled.cp437(true);
    oled.setTextSize(1);
    oled.setTextWrap(false);
    oled.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
    oled.clearDisplay();
    oled.display();
    available_ = true;

    char msg[48];
    snprintf(msg, sizeof(msg), "128x64 SSD1306 OLED initialized at 0x%02X", address);
    logInfo("OLED", msg);
    return true;
}

void OledDisplay::showMessage(const char* line0, const char* line1, const char* line2, const char* line3)
{
    drawLine(0, line0);
    drawLine(1, line1);
    drawLine(2, line2);
    drawLine(3, line3);
    flush();
}

void OledDisplay::showLine(uint8_t row, const char* text)
{
    drawLine(row, text);
    flush();
}

void OledDisplay::drawLine(uint8_t row, const char* text)
{
    if (!available_ || row >= OLED_ROWS)
        return;
    char padded[OLED_COLUMNS + 1];
    snprintf(padded, sizeof(padded), "%-*.*s", OLED_COLUMNS, OLED_COLUMNS, text);
    // Redraw only changed lines and push the buffer only when something changed.
    if (strcmp(padded, shown_[row]) == 0)
        return;
    oled.fillRect(0, row * LINE_HEIGHT_PX, OLED_WIDTH, LINE_HEIGHT_PX, SSD1306_BLACK);
    oled.setCursor(0, row * LINE_HEIGHT_PX + TEXT_OFFSET_PX);
    oled.print(padded);
    strcpy(shown_[row], padded);
    dirty_ = true;
}

void OledDisplay::flush()
{
    if (!available_ || !dirty_)
        return;
    oled.display();
    dirty_ = false;
}

void OledDisplay::showDashboard(const DisplayStatus& status)
{
    if (!available_)
        return;
    char alerts[MAX_ALERTS][OLED_COLUMNS + 1];
    const uint8_t alertCount = collectAlerts(status, alerts);
    const uint8_t pageCount = alertCount > 0 ? 3 : 2;

    const uint32_t now = millis();
    if (pageShownAtMs_ == 0)
        pageShownAtMs_ = now;
    else if (now - pageShownAtMs_ >= OLED_PAGE_INTERVAL_MS)
    {
        page_ = (page_ + 1) % pageCount;
        pageShownAtMs_ = now;
    }
    if (page_ >= pageCount)
        page_ = 0;

    if (page_ == 0)
        showSensorPage(status);
    else if (page_ == 1)
        showStatusPage(status);
    else
    {
        char title[OLED_COLUMNS + 1];
        snprintf(title, sizeof(title), "!! ALERTS (%u) !!", alertCount);
        drawLine(0, title);
        for (uint8_t i = 0; i < MAX_ALERTS; ++i)
            drawLine(i + 1, i < alertCount ? alerts[i] : "");
    }
    flush();
}

void OledDisplay::showSensorPage(const DisplayStatus& status)
{
    const SensorSnapshot& s = status.sensors;
    if (s.sampledAtMs == 0)
    {
        drawLine(0, "Reading sensors");
        drawLine(1, "Please wait...");
        drawLine(2, "");
        drawLine(3, "");
        return;
    }
    char line[OLED_COLUMNS + 1];

    if (soilSensorFault(s) || s.soilMoisturePercent < 0)
        snprintf(line, sizeof(line), "Soil: ERROR");
    else
    {
        const char* level = s.soilMoisturePercent <= SOIL_DRY_THRESHOLD_PERCENT ? "DRY"
                            : s.soilMoisturePercent >= SOIL_WET_STOP_PERCENT ? "WET"
                                                                              : "OK";
        snprintf(line, sizeof(line), "Soil:%5.1f%% %s", s.soilMoisturePercent, level);
    }
    drawLine(0, line);

    if (s.temperatureValid)
        snprintf(line, sizeof(line), "Temp:%5.1f%sC", s.temperatureC, DEGREE);
    else
        snprintf(line, sizeof(line), "Temp: ERROR");
    drawLine(1, line);

    snprintf(line, sizeof(line), "Flow:%5.2f L/min", s.flowLitresPerMinute);
    drawLine(2, line);
    snprintf(line, sizeof(line), "Total:%7.1f L", s.totalLitres);
    drawLine(3, line);
}

void OledDisplay::showStatusPage(const DisplayStatus& status)
{
    char line[OLED_COLUMNS + 1];
    const bool online = status.wifiConnected && status.mqttConnected;
    // Mirrors ControlTask: without Wi-Fi+MQTT the pump runs on local soil control.
    if (!online)
        drawLine(0, "Mode: LOCAL AUTO");
    else
        drawLine(0, status.mode == OperatingMode::MANUAL ? "Mode: MANUAL" : "Mode: AUTO");

    if (status.pumpOn)
    {
        const uint32_t runSeconds = (millis() - status.pumpStartedAtMs) / 1000UL;
        snprintf(line, sizeof(line), "Pump: ON  %02lu:%02lu",
                 static_cast<unsigned long>(runSeconds / 60UL), static_cast<unsigned long>(runSeconds % 60UL));
        drawLine(1, line);
    }
    else
        drawLine(1, status.pumpFault != PumpFault::NONE ? "Pump: OFF FAULT" : "Pump: OFF");

    snprintf(line, sizeof(line), "WiFi:%s MQTT:%s",
             status.wifiConnected ? "OK" : "NO", status.mqttConnected ? "OK" : "NO");
    drawLine(2, line);

    const uint32_t up = millis() / 1000UL;
    snprintf(line, sizeof(line), "Up %lud %02lu:%02lu:%02lu",
             static_cast<unsigned long>(up / 86400UL), static_cast<unsigned long>((up / 3600UL) % 24UL),
             static_cast<unsigned long>((up / 60UL) % 60UL), static_cast<unsigned long>(up % 60UL));
    drawLine(3, line);
}
