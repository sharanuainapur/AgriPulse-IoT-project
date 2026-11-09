#pragma once
#include <Arduino.h>
#include "app_state.h"
#include "config.h"

// The part of SharedState the dashboard shows, copied out under the mutex.
struct DisplayStatus {
    SensorSnapshot sensors;
    OperatingMode mode = OperatingMode::AUTO;
    bool wifiConnected = false;
    bool mqttConnected = false;
    WifiSetupStatus wifiSetupStatus = WifiSetupStatus::STARTING;
    PortalReason portalReason = PortalReason::NONE;
    uint32_t wifiConnectStartedAtMs = 0;
    int8_t wifiRssi = 0;
    int8_t mqttState = -1;
    uint8_t portalClients = 0;
    char wifiSsid[33] = {};
    char wifiPortalSsid[33] = {};
    char wifiPendingSsid[33] = {};
    char wifiIp[16] = {};
    bool pumpOn = false;
    PumpFault pumpFault = PumpFault::NONE;
    uint32_t pumpStartedAtMs = 0;
};

// 128x64 SSD1306 UI.
//   y 0..15  status bar: mode chip, page title, pump / cloud / Wi-Fi icons
//   y16..63  content: dashboard pages, or a Wi-Fi setup screen when needed
// After boot only DisplayTask calls into this class.
class OledDisplay {
public:
    bool begin();
    bool available() const { return available_; }
    // Boot/progress screen: line0 is the title, lines 1..3 are body text.
    void showMessage(const char* line0, const char* line1 = "", const char* line2 = "", const char* line3 = "");
    void showLine(uint8_t row, const char* text);
    void showDashboard(const DisplayStatus& status);

private:
    enum class Page : uint8_t { OVERVIEW = 0, SENSORS, NETWORK, ALERTS, SETUP };

    void renderMessage();
    void drawStatusBar(const DisplayStatus& s, const char* chip, const char* title, uint32_t now);
    void drawOverviewPage(const DisplayStatus& s, uint32_t now);
    void drawSensorsPage(const DisplayStatus& s);
    void drawNetworkPage(const DisplayStatus& s, uint32_t now);
    void drawAlertsPage(const DisplayStatus& s);
    void drawStartingScreen(const DisplayStatus& s, uint32_t now);
    void drawConnectingScreen(const DisplayStatus& s, uint32_t now, bool newNetwork);
    void drawPortalScreen(const DisplayStatus& s, uint32_t now);
    void drawConnectedScreen(const DisplayStatus& s, uint32_t now);
    void drawErrorScreen(const DisplayStatus& s, uint32_t now);
    Page pickPage(const DisplayStatus& s, uint8_t alertCount, uint32_t now);
    void flush();  // pushes the frame over I2C only when it changed

    bool available_ = false;
    uint32_t frameHash_ = 0;
    char message_[4][24] = {};
    uint8_t page_ = 0;
    uint32_t pageShownAtMs_ = 0;
    uint8_t lastAlertCount_ = 0;
    bool wasWifiConnected_ = false;
    bool hadWifiSinceBoot_ = false;
    uint32_t connectedSplashUntilMs_ = 0;
};
