#include "oled_display.h"
#include "logger.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

static Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire, -1); // -1: module has no reset pin

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------
static constexpr int16_t BAR_ICON_Y = 3;   // status-bar icons/text sit in y 3..12
static constexpr int16_t BAR_RULE_Y = 14;  // thin rule under the status bar
static constexpr int16_t ICON_PUMP_X = 79;
static constexpr int16_t ICON_MQTT_X = 88;   // "MQTT" badge, 27 px wide
static constexpr int16_t ICON_WIFI_X = 117;
static constexpr int16_t ROW_Y[4] = {18, 30, 42, 54}; // four text rows in the content area
static const char DEGREE[] = "\xF8";                  // degree sign in the CP437 font

// ---------------------------------------------------------------------------
// Drawing helpers
// ---------------------------------------------------------------------------
static int16_t textWidth(const char* t, uint8_t size = 1)
{
    const size_t n = strlen(t);
    return n == 0 ? 0 : static_cast<int16_t>(n * 6 * size - size);
}

static void text(int16_t x, int16_t y, const char* t, uint8_t size = 1, uint16_t color = SSD1306_WHITE)
{
    oled.setTextSize(size);
    oled.setTextColor(color);
    oled.setCursor(x, y);
    oled.print(t);
}

static void textRight(int16_t xRight, int16_t y, const char* t, uint8_t size = 1)
{
    text(xRight - textWidth(t, size), y, t, size);
}

static void textCenter(int16_t y, const char* t, uint8_t size = 1)
{
    text((OLED_WIDTH - textWidth(t, size)) / 2, y, t, size);
}

// Copies `in` into `out`, shortening it with ".." if it exceeds maxChars.
static void fitText(char* out, size_t outSize, const char* in, uint8_t maxChars)
{
    if (strlen(in) <= maxChars)
        snprintf(out, outSize, "%s", in);
    else
        snprintf(out, outSize, "%.*s..", maxChars - 2, in);
}

// Rounded, filled label with black text. Returns its width.
static int16_t chip(int16_t x, int16_t y, const char* label)
{
    const int16_t w = textWidth(label) + 5;
    oled.fillRoundRect(x, y, w, 10, 2, SSD1306_WHITE);
    text(x + 3, y + 1, label, 1, SSD1306_BLACK);
    return w;
}

static void progressBar(int16_t x, int16_t y, int16_t w, int16_t h, float fraction)
{
    fraction = constrain(fraction, 0.0f, 1.0f);
    oled.drawRect(x, y, w, h, SSD1306_WHITE);
    const int16_t fill = static_cast<int16_t>((w - 4) * fraction);
    if (fill > 0)
        oled.fillRect(x + 2, y + 2, fill, h - 4, SSD1306_WHITE);
}

static void dottedHLine(int16_t y)
{
    for (int16_t x = 0; x < OLED_WIDTH; x += 3)
        oled.drawPixel(x, y, SSD1306_WHITE);
}

// Eight-dot spinner; the bright dot moves every refresh.
static void spinner(int16_t cx, int16_t cy, uint32_t now)
{
    static const int8_t OFFSETS[8][2] = {{0, -6}, {4, -4}, {6, 0}, {4, 4}, {0, 6}, {-4, 4}, {-6, 0}, {-4, -4}};
    const uint8_t head = (now / OLED_REFRESH_INTERVAL_MS) % 8;
    for (uint8_t i = 0; i < 8; ++i)
    {
        const int16_t x = cx + OFFSETS[i][0], y = cy + OFFSETS[i][1];
        if (i == head)
            oled.fillCircle(x, y, 1, SSD1306_WHITE);
        else
            oled.drawPixel(x, y, SSD1306_WHITE);
    }
}

static bool blinkOn(uint32_t now, uint32_t periodMs = 500)
{
    return (now / periodMs) % 2 == 0;
}

// ---------------------------------------------------------------------------
// Icons (status bar)
// ---------------------------------------------------------------------------
enum class WifiIcon : uint8_t { CONNECTED, CONNECTING, OFFLINE, ACCESS_POINT };

static uint8_t rssiBars(int8_t rssi)
{
    if (rssi >= -60) return 4;
    if (rssi >= -70) return 3;
    if (rssi >= -80) return 2;
    return 1;
}

static void wifiIcon(int16_t x, int16_t y, WifiIcon state, int8_t rssi, uint32_t now)
{
    if (state == WifiIcon::ACCESS_POINT)
    {
        text(x - 1, y + 1, "AP");
        return;
    }
    uint8_t bars = 0;
    if (state == WifiIcon::CONNECTED)
        bars = rssiBars(rssi);
    else if (state == WifiIcon::CONNECTING)
        bars = (now / OLED_REFRESH_INTERVAL_MS) % 5;
    for (uint8_t i = 0; i < 4; ++i)
    {
        const int16_t bx = x + i * 3;
        const int16_t h = 3 + i * 2;
        if (i < bars)
            oled.fillRect(bx, y + 10 - h, 2, h, SSD1306_WHITE);
        else
            oled.drawFastHLine(bx, y + 9, 2, SSD1306_WHITE);
    }
    if (state == WifiIcon::OFFLINE)
    {
        oled.drawLine(x, y, x + 4, y + 4, SSD1306_WHITE);
        oled.drawLine(x + 4, y, x, y + 4, SSD1306_WHITE);
    }
}

// "MQTT" badge: solid = online, blinking outline = connecting,
// outline with a strike-through = offline (no Wi-Fi).
enum class MqttBadge : uint8_t { ONLINE, CONNECTING, OFFLINE };

static void mqttBadge(int16_t x, int16_t y, MqttBadge state, uint32_t now)
{
    static constexpr int16_t W = 27, H = 10;
    if (state == MqttBadge::ONLINE)
    {
        oled.fillRoundRect(x, y, W, H, 2, SSD1306_WHITE);
        text(x + 2, y + 1, "MQTT", 1, SSD1306_BLACK);
        return;
    }
    if (state == MqttBadge::CONNECTING && !blinkOn(now))
    {
        text(x + 2, y + 1, "MQTT");
        return;
    }
    oled.drawRoundRect(x, y, W, H, 2, SSD1306_WHITE);
    text(x + 2, y + 1, "MQTT");
    if (state == MqttBadge::OFFLINE)
        oled.drawFastHLine(x, y + 4, W, SSD1306_WHITE);
}

// Plain-language reason for the MQTT state, from PubSubClient's state code.
static const char* mqttStateText(const DisplayStatus& s, uint32_t now)
{
    if (s.mqttConnected)
        return "Connected";
    if (!s.wifiConnected)
        return "Offline (no WiFi)";
    switch (s.mqttState)
    {
    case -4: return "No reply";       // MQTT_CONNECTION_TIMEOUT
    case -3: return "Link lost";      // MQTT_CONNECTION_LOST
    case -2: return "No server";      // MQTT_CONNECT_FAILED (TCP failed)
    case 1:  return "Bad protocol";   // MQTT_CONNECT_BAD_PROTOCOL
    case 2:  return "ID rejected";    // MQTT_CONNECT_BAD_CLIENT_ID
    case 3:  return "Server busy";    // MQTT_CONNECT_UNAVAILABLE
    case 4:  return "Bad login";      // MQTT_CONNECT_BAD_CREDENTIALS
    case 5:  return "Not authorized"; // MQTT_CONNECT_UNAUTHORIZED
    default:
        static const char* DOTS[4] = {"Connecting", "Connecting.", "Connecting..", "Connecting..."};
        return DOTS[(now / 500UL) % 4];
    }
}

static void dropIcon(int16_t x, int16_t y)
{
    oled.fillTriangle(x + 3, y, x, y + 5, x + 6, y + 5, SSD1306_WHITE);
    oled.fillCircle(x + 3, y + 6, 3, SSD1306_WHITE);
}

// Numbered step badge for the setup guide: done = tick, active = blinking.
static void stepBadge(int16_t x, int16_t y, uint8_t number, bool done, bool active, uint32_t now)
{
    if (done)
    {
        oled.fillRoundRect(x, y, 9, 9, 2, SSD1306_WHITE);
        oled.drawLine(x + 2, y + 4, x + 3, y + 6, SSD1306_BLACK);
        oled.drawLine(x + 3, y + 6, x + 6, y + 2, SSD1306_BLACK);
        return;
    }
    char digit[2] = {static_cast<char>('0' + number), '\0'};
    if (active && blinkOn(now))
    {
        oled.fillRoundRect(x, y, 9, 9, 2, SSD1306_WHITE);
        text(x + 2, y + 1, digit, 1, SSD1306_BLACK);
    }
    else
    {
        oled.drawRoundRect(x, y, 9, 9, 2, SSD1306_WHITE);
        text(x + 2, y + 1, digit);
    }
}

// ---------------------------------------------------------------------------
// Alerts
// ---------------------------------------------------------------------------
struct AlertRow {
    const char* text;
    bool hint; // follow-up advice for the alert above (no warning badge)
};
static constexpr uint8_t MAX_ALERT_ROWS = 4;

static bool soilSensorFault(const SensorSnapshot& s)
{
    return s.soilAdc <= SOIL_ADC_FAULT_LOW || s.soilAdc >= SOIL_ADC_FAULT_HIGH;
}

// Fills rows with the active problems, most serious first. Returns the number
// of problems (hints are not counted); rowCount receives the rows written.
static uint8_t collectAlerts(const DisplayStatus& s, AlertRow rows[MAX_ALERT_ROWS], uint8_t& rowCount)
{
    uint8_t problems = 0;
    rowCount = 0;
    auto add = [&](const char* t, bool hint) {
        if (!hint)
            ++problems;
        if (rowCount < MAX_ALERT_ROWS)
            rows[rowCount++] = {t, hint};
    };
    if (s.pumpFault == PumpFault::NO_FLOW)
    {
        add("Pump stop: no flow", false);
        add("Check water supply", true);
    }
    else if (s.pumpFault == PumpFault::MAX_RUNTIME)
    {
        add("Pump stop: max time", false);
    }
    if (s.pumpFault != PumpFault::NONE)
        add("Reboot/app to clear", true);
    if (s.sensors.sampledAtMs != 0)
    {
        if (soilSensorFault(s.sensors))
            add("Soil probe fault", false);
        if (!s.sensors.temperatureValid)
            add("Temp sensor missing", false);
    }
    if (!s.wifiConnected)
        add("No WiFi: local auto", false);
    else if (!s.mqttConnected)
    {
        add("MQTT offline", false);
        add("App control paused", true);
    }
    return problems;
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------
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
    oled.setTextWrap(false);
    oled.clearDisplay();
    oled.display();
    available_ = true;

    char msg[48];
    snprintf(msg, sizeof(msg), "128x64 SSD1306 OLED initialized at 0x%02X", address);
    logInfo("OLED", msg);
    return true;
}

void OledDisplay::flush()
{
    // Skip the ~25 ms I2C transfer when the frame is identical to the last one.
    const uint8_t* buf = oled.getBuffer();
    uint32_t hash = 2166136261UL;
    for (size_t i = 0; i < static_cast<size_t>(OLED_WIDTH) * OLED_HEIGHT / 8; ++i)
        hash = (hash ^ buf[i]) * 16777619UL;
    if (hash == frameHash_)
        return;
    frameHash_ = hash;
    oled.display();
}

// ---------------------------------------------------------------------------
// Boot / message screen
// ---------------------------------------------------------------------------
void OledDisplay::showMessage(const char* line0, const char* line1, const char* line2, const char* line3)
{
    const char* lines[4] = {line0, line1, line2, line3};
    for (uint8_t i = 0; i < 4; ++i)
    {
        const char* t = lines[i] ? lines[i] : "";
        while (*t == ' ')
            ++t;
        snprintf(message_[i], sizeof(message_[i]), "%s", t);
    }
    renderMessage();
}

void OledDisplay::showLine(uint8_t row, const char* text)
{
    if (row >= 4)
        return;
    while (text && *text == ' ')
        ++text;
    snprintf(message_[row], sizeof(message_[row]), "%s", text ? text : "");
    renderMessage();
}

void OledDisplay::renderMessage()
{
    if (!available_)
        return;
    oled.clearDisplay();
    // Title bar with the AgriPulse drop logo.
    const int16_t titleW = textWidth(message_[0]);
    const int16_t x = (OLED_WIDTH - titleW - 10) / 2;
    dropIcon(x, BAR_ICON_Y);
    text(x + 10, BAR_ICON_Y + 1, message_[0]);
    oled.drawFastHLine(0, BAR_RULE_Y, OLED_WIDTH, SSD1306_WHITE);

    static const int16_t BODY_Y[3] = {21, 35, 49};
    for (uint8_t i = 1; i < 4; ++i)
    {
        const char* t = message_[i];
        if (t[0] == '!')
        {
            // Warnings (e.g. "!Last: crash") are shown as a highlighted chip.
            const int16_t w = textWidth(t + 1) + 5;
            chip((OLED_WIDTH - w) / 2, BODY_Y[i - 1] - 1, t + 1);
        }
        else
            textCenter(BODY_Y[i - 1], t);
    }
    flush();
}

// ---------------------------------------------------------------------------
// Status bar
// ---------------------------------------------------------------------------
void OledDisplay::drawStatusBar(const DisplayStatus& s, const char* chipLabel, const char* title, uint32_t now)
{
    int16_t x = 0;
    if (chipLabel && chipLabel[0])
        x = chip(0, BAR_ICON_Y, chipLabel) + 4;
    text(x, BAR_ICON_Y + 1, title);

    // Pump: blinking "!" on a fault, a dripping drop while watering.
    if (s.pumpFault != PumpFault::NONE)
    {
        if (blinkOn(now))
        {
            oled.fillRoundRect(ICON_PUMP_X, BAR_ICON_Y, 7, 10, 1, SSD1306_WHITE);
            text(ICON_PUMP_X + 1, BAR_ICON_Y + 1, "!", 1, SSD1306_BLACK);
        }
    }
    else if (s.pumpOn)
        dropIcon(ICON_PUMP_X, BAR_ICON_Y + (blinkOn(now) ? 0 : 1));

    mqttBadge(ICON_MQTT_X, BAR_ICON_Y, s.mqttConnected ? MqttBadge::ONLINE
                                       : s.wifiConnected ? MqttBadge::CONNECTING
                                                         : MqttBadge::OFFLINE, now);

    WifiIcon wifi = WifiIcon::OFFLINE;
    if (s.wifiConnected)
        wifi = WifiIcon::CONNECTED;
    else if (s.wifiSetupStatus == WifiSetupStatus::PORTAL)
        wifi = WifiIcon::ACCESS_POINT;
    else if (s.wifiSetupStatus == WifiSetupStatus::CONNECTING ||
             s.wifiSetupStatus == WifiSetupStatus::VERIFYING ||
             s.wifiSetupStatus == WifiSetupStatus::STARTING)
        wifi = WifiIcon::CONNECTING;
    wifiIcon(ICON_WIFI_X, BAR_ICON_Y, wifi, s.wifiRssi, now);

    oled.drawFastHLine(0, BAR_RULE_Y, OLED_WIDTH, SSD1306_WHITE);
}

static const char* modeChip(const DisplayStatus& s)
{
    // Mirrors ControlTask: without Wi-Fi + MQTT the pump runs on local soil control.
    if (!s.wifiConnected || !s.mqttConnected)
        return "LOCAL";
    return s.mode == OperatingMode::MANUAL ? "MANUAL" : "AUTO";
}

// ---------------------------------------------------------------------------
// Dashboard pages
// ---------------------------------------------------------------------------
void OledDisplay::drawOverviewPage(const DisplayStatus& s, uint32_t now)
{
    drawStatusBar(s, modeChip(s), "HOME", now);
    const SensorSnapshot& sn = s.sensors;
    if (sn.sampledAtMs == 0)
    {
        spinner(64, 32, now);
        textCenter(48, "Reading sensors...");
        return;
    }
    char buf[24];

    // Left: soil moisture, big.
    text(0, ROW_Y[0], "SOIL MOISTURE");
    const bool soilOk = !soilSensorFault(sn) && sn.soilMoisturePercent >= 0;
    if (soilOk)
    {
        snprintf(buf, sizeof(buf), "%.0f%%", sn.soilMoisturePercent);
        text(0, 28, buf, 2);
        const char* level = sn.soilMoisturePercent <= SOIL_DRY_THRESHOLD_PERCENT ? "DRY"
                            : sn.soilMoisturePercent >= SOIL_WET_STOP_PERCENT  ? "WET"
                                                                               : "OK";
        chip(textWidth(buf, 2) + 5, 31, level);
    }
    else
    {
        text(0, 28, "--", 2);
        chip(30, 31, "CHECK");
    }

    // Right: pump state, big. "ON" is shown inverted so it stands out.
    textRight(127, ROW_Y[0], "PUMP");
    if (s.pumpOn)
    {
        oled.fillRoundRect(98, 25, 30, 18, 3, SSD1306_WHITE);
        text(102, 27, "ON", 2, SSD1306_BLACK);
    }
    else
        textRight(127, 28, "OFF", 2);

    // Moisture gauge with tick marks at the start (dry) and stop (wet) levels.
    const float pct = soilOk ? sn.soilMoisturePercent : 0.0f;
    progressBar(0, 46, OLED_WIDTH, 7, pct / 100.0f);
    const int16_t dryX = 2 + static_cast<int16_t>(124 * SOIL_DRY_THRESHOLD_PERCENT / 100.0f);
    const int16_t wetX = 2 + static_cast<int16_t>(124 * SOIL_WET_STOP_PERCENT / 100.0f);
    oled.drawFastVLine(dryX, 44, 2, SSD1306_WHITE);
    oled.drawFastVLine(wetX, 44, 2, SSD1306_WHITE);

    // Bottom line: what the system is doing and why.
    if (s.pumpFault == PumpFault::NO_FLOW)
        textCenter(56, "STOPPED: no flow");
    else if (s.pumpFault == PumpFault::MAX_RUNTIME)
        textCenter(56, "STOPPED: ran too long");
    else if (s.pumpOn)
    {
        const uint32_t runS = (now - s.pumpStartedAtMs) / 1000UL;
        snprintf(buf, sizeof(buf), "Watering %02lu:%02lu",
                 static_cast<unsigned long>(runS / 60UL), static_cast<unsigned long>(runS % 60UL));
        text(0, 56, buf);
        snprintf(buf, sizeof(buf), "%.1fL/m", sn.flowLitresPerMinute);
        textRight(127, 56, buf);
    }
    else if (s.mode == OperatingMode::MANUAL && s.wifiConnected && s.mqttConnected)
        textCenter(56, "Manual: app control");
    else if (!soilOk)
        textCenter(56, "Check soil probe");
    else
    {
        snprintf(buf, sizeof(buf), "Waters below %.0f%%", SOIL_DRY_THRESHOLD_PERCENT);
        textCenter(56, buf);
    }
}

void OledDisplay::drawSensorsPage(const DisplayStatus& s)
{
    drawStatusBar(s, modeChip(s), "SENSOR", millis());
    const SensorSnapshot& sn = s.sensors;
    const bool sampled = sn.sampledAtMs != 0;
    char buf[24];

    text(0, ROW_Y[0], "Temperature");
    if (sampled && sn.temperatureValid)
        snprintf(buf, sizeof(buf), "%.1f%sC", sn.temperatureC, DEGREE);
    else
        snprintf(buf, sizeof(buf), sampled ? "ERROR" : "--");
    textRight(127, ROW_Y[0], buf);

    text(0, ROW_Y[1], "Water flow");
    snprintf(buf, sizeof(buf), "%.2f L/min", sn.flowLitresPerMinute);
    textRight(127, ROW_Y[1], sampled ? buf : "--");

    text(0, ROW_Y[2], "Water used");
    snprintf(buf, sizeof(buf), "%.1f L", sn.totalLitres);
    textRight(127, ROW_Y[2], sampled ? buf : "--");

    text(0, ROW_Y[3], "Soil sensor");
    if (!sampled)
        snprintf(buf, sizeof(buf), "--");
    else if (soilSensorFault(sn))
        snprintf(buf, sizeof(buf), "FAULT %d", sn.soilAdc);
    else
        snprintf(buf, sizeof(buf), "ADC %d", sn.soilAdc);
    textRight(127, ROW_Y[3], buf);

    dottedHLine(27);
    dottedHLine(39);
    dottedHLine(51);
}

void OledDisplay::drawNetworkPage(const DisplayStatus& s, uint32_t now)
{
    drawStatusBar(s, modeChip(s), "NET", now);
    char buf[24];

    text(0, ROW_Y[0], "WiFi");
    if (s.wifiConnected)
        fitText(buf, sizeof(buf), s.wifiSsid, 15);
    else
        snprintf(buf, sizeof(buf), s.wifiSetupStatus == WifiSetupStatus::PORTAL ? "Setup mode" : "Not connected");
    textRight(127, ROW_Y[0], buf);

    text(0, ROW_Y[1], "IP");
    textRight(127, ROW_Y[1], s.wifiConnected ? s.wifiIp : "-");

    text(0, ROW_Y[2], "Signal");
    if (s.wifiConnected)
    {
        static const char* QUALITY[5] = {"", "Weak", "Fair", "Good", "Strong"};
        snprintf(buf, sizeof(buf), "%ddBm %s", s.wifiRssi, QUALITY[rssiBars(s.wifiRssi)]);
    }
    else
        snprintf(buf, sizeof(buf), "-");
    textRight(127, ROW_Y[2], buf);

    text(0, ROW_Y[3], "MQTT");
    textRight(127, ROW_Y[3], mqttStateText(s, now));

    dottedHLine(27);
    dottedHLine(39);
    dottedHLine(51);
}

void OledDisplay::drawAlertsPage(const DisplayStatus& s)
{
    AlertRow rows[MAX_ALERT_ROWS];
    uint8_t rowCount = 0;
    const uint8_t problems = collectAlerts(s, rows, rowCount);
    char title[12];
    snprintf(title, sizeof(title), "ALERT %u", problems);
    drawStatusBar(s, "!", title, millis());

    for (uint8_t i = 0; i < rowCount; ++i)
    {
        const int16_t y = ROW_Y[i];
        if (!rows[i].hint)
        {
            oled.fillTriangle(0, y + 7, 4, y - 1, 8, y + 7, SSD1306_WHITE);
            text(3, y, "!", 1, SSD1306_BLACK);
        }
        text(12, y, rows[i].text);
    }
}

// ---------------------------------------------------------------------------
// Wi-Fi setup screens
// ---------------------------------------------------------------------------
void OledDisplay::drawStartingScreen(const DisplayStatus& s, uint32_t now)
{
    drawStatusBar(s, "WiFi", "STARTING", now);
    spinner(64, 32, now);
    textCenter(48, "Starting WiFi...");
}

void OledDisplay::drawConnectingScreen(const DisplayStatus& s, uint32_t now, bool newNetwork)
{
    drawStatusBar(s, "WiFi", "JOINING", now);
    textCenter(ROW_Y[0], newNetwork ? "Trying new network" : "Connecting to WiFi");

    char ssid[24], quoted[28];
    fitText(ssid, sizeof(ssid), s.wifiPendingSsid[0] ? s.wifiPendingSsid : "saved network", 17);
    snprintf(quoted, sizeof(quoted), "\"%s\"", ssid);
    textCenter(ROW_Y[1], quoted);

    const uint32_t elapsed = s.wifiConnectStartedAtMs ? now - s.wifiConnectStartedAtMs : 0;
    progressBar(0, 41, 100, 9, static_cast<float>(elapsed) / WIFI_CONNECT_TIMEOUT_MS);
    char secs[8];
    snprintf(secs, sizeof(secs), "%lus", static_cast<unsigned long>(elapsed / 1000UL));
    textRight(127, 42, secs);

    textCenter(56, newNetwork ? "Please wait..." : "BOOT 3s: change WiFi");
}

void OledDisplay::drawPortalScreen(const DisplayStatus& s, uint32_t now)
{
    drawStatusBar(s, "WiFi", "SETUP", now);

    // Why setup is needed. A failed attempt is highlighted.
    switch (s.portalReason)
    {
    case PortalReason::NEW_WIFI_FAILED:
        if (blinkOn(now, 700))
        {
            oled.fillRect(0, 16, OLED_WIDTH, 10, SSD1306_WHITE);
            text((OLED_WIDTH - textWidth("Failed! Check pass")) / 2, 17, "Failed! Check pass", 1, SSD1306_BLACK);
        }
        else
            textCenter(17, "Failed! Check pass");
        break;
    case PortalReason::SAVED_UNREACHABLE:
        textCenter(17, "Saved WiFi not found");
        break;
    case PortalReason::USER_RESET:
        textCenter(17, "WiFi was reset");
        break;
    default:
        textCenter(17, "Connect me to WiFi");
        break;
    }

    // Step-by-step guide; step 1 ticks off once a phone joins the setup AP.
    const bool phoneJoined = s.portalClients > 0;
    char line[24];
    snprintf(line, sizeof(line), "Join %s", s.wifiPortalSsid);
    stepBadge(0, 28, 1, phoneJoined, !phoneJoined, now);
    text(13, 29, line);
    stepBadge(0, 40, 2, false, phoneJoined, now);
    text(13, 41, "Open 192.168.4.1");
    stepBadge(0, 52, 3, false, false, now);
    text(13, 53, "Pick WiFi + Save");
}

void OledDisplay::drawConnectedScreen(const DisplayStatus& s, uint32_t now)
{
    drawStatusBar(s, "WiFi", "READY", now);
    // Big tick in a circle.
    oled.fillCircle(12, 32, 11, SSD1306_WHITE);
    for (int8_t d = 0; d < 2; ++d)
    {
        oled.drawLine(6, 32 + d, 10, 36 + d, SSD1306_BLACK);
        oled.drawLine(10, 36 + d, 18, 27 + d, SSD1306_BLACK);
    }
    char buf[24];
    text(30, ROW_Y[0], "Connected!");
    fitText(buf, sizeof(buf), s.wifiSsid, 16);
    text(30, ROW_Y[1], buf);
    text(30, ROW_Y[2], s.wifiIp);
    char mqttLine[24];
    snprintf(mqttLine, sizeof(mqttLine), "MQTT: %s", mqttStateText(s, now));
    textCenter(56, mqttLine);
}

void OledDisplay::drawErrorScreen(const DisplayStatus& s, uint32_t now)
{
    drawStatusBar(s, "WiFi", "ERROR", now);
    textCenter(ROW_Y[0], "WiFi setup failed");
    textCenter(ROW_Y[1], "Storage (NVS) error");
    textCenter(ROW_Y[2], "Pump runs locally");
    textCenter(ROW_Y[3], "Please restart");
}

// ---------------------------------------------------------------------------
// Screen selection
// ---------------------------------------------------------------------------
OledDisplay::Page OledDisplay::pickPage(const DisplayStatus& s, uint8_t alertCount, uint32_t now)
{
    const uint8_t pageCount = alertCount > 0 ? 4 : 3;
    if (alertCount > lastAlertCount_)
    {
        // A new problem appeared: show it right away.
        page_ = static_cast<uint8_t>(Page::ALERTS);
        pageShownAtMs_ = now;
    }
    else if (pageShownAtMs_ == 0)
        pageShownAtMs_ = now;
    else if (now - pageShownAtMs_ >= OLED_PAGE_INTERVAL_MS)
    {
        page_ = (page_ + 1) % pageCount;
        pageShownAtMs_ = now;
    }
    lastAlertCount_ = alertCount;
    if (page_ >= pageCount)
        page_ = 0;
    return static_cast<Page>(page_);
}

void OledDisplay::showDashboard(const DisplayStatus& s)
{
    if (!available_)
        return;
    const uint32_t now = millis();

    if (s.wifiConnected && !wasWifiConnected_)
    {
        connectedSplashUntilMs_ = now + OLED_CONNECTED_SPLASH_MS;
        hadWifiSinceBoot_ = true;
    }
    wasWifiConnected_ = s.wifiConnected;

    AlertRow rows[MAX_ALERT_ROWS];
    uint8_t rowCount = 0;
    const uint8_t alertCount = collectAlerts(s, rows, rowCount);

    oled.clearDisplay();
    switch (s.wifiSetupStatus)
    {
    case WifiSetupStatus::ERROR:
        drawErrorScreen(s, now);
        break;
    case WifiSetupStatus::STARTING:
        drawStartingScreen(s, now);
        break;
    case WifiSetupStatus::VERIFYING:
        drawConnectingScreen(s, now, true);
        break;
    case WifiSetupStatus::PORTAL:
        // While someone is configuring, keep the guide up. Otherwise alternate
        // with the home page so the field status stays visible.
        if (s.portalClients > 0 || (now / OLED_PAGE_INTERVAL_MS) % 2 == 0)
            drawPortalScreen(s, now);
        else
            drawOverviewPage(s, now);
        break;
    default:
        if (s.wifiSetupStatus == WifiSetupStatus::CONNECTING && !hadWifiSinceBoot_)
        {
            drawConnectingScreen(s, now, false);
            break;
        }
        if (s.wifiConnected && static_cast<int32_t>(connectedSplashUntilMs_ - now) > 0)
        {
            drawConnectedScreen(s, now);
            break;
        }
        switch (pickPage(s, alertCount, now))
        {
        case Page::SENSORS:
            drawSensorsPage(s);
            break;
        case Page::NETWORK:
            drawNetworkPage(s, now);
            break;
        case Page::ALERTS:
            drawAlertsPage(s);
            break;
        default:
            drawOverviewPage(s, now);
            break;
        }
        break;
    }
    flush();
}
