#include "network_manager.h"
#include "config.h"
#include "logger.h"
#include <Preferences.h>

static MqttMessageCallback userCommandCallback = nullptr;

void NetworkManager::begin() {
    WiFi.mode(WIFI_STA);
    // Modem sleep: the radio dozes between AP beacons while staying associated,
    // so MQTT commands still arrive with only beacon-interval latency.
    WiFi.setSleep(WIFI_PS_MIN_MODEM);
    WiFi.setAutoReconnect(true);
    mqttClient_.setServer(MQTT_HOST, MQTT_PORT);
    mqttClient_.setBufferSize(MQTT_BUFFER_SIZE);
    mqttClient_.setKeepAlive(30);
    mqttClient_.setSocketTimeout(5);
    mqttClient_.setCallback(NetworkManager::callbackThunk);
    pinMode(WIFI_RESET_BUTTON_PIN, INPUT_PULLUP);

    snprintf(wifiPortalSsid_, sizeof(wifiPortalSsid_), "AgriPulse-%04X",
             static_cast<unsigned int>(ESP.getEfuseMac() & 0xFFFF));
    wifiManager_.setConfigPortalBlocking(false);
    wifiManager_.setConfigPortalTimeout(0);
    wifiManager_.setConnectTimeout(WIFI_CONNECT_TIMEOUT_MS / 1000UL);
    wifiManager_.setPreSaveConfigCallback([this]() { onPortalSave(); });

    Preferences preferences;
    if (!preferences.begin("agripulse", false)) {
        logError("Network", "Could not open NVS; Wi-Fi setup cannot continue");
        wifiSetupStatus_ = WifiSetupStatus::ERROR;
        return;
    }
    if (!preferences.getBool("wifi_init", false)) {
        logInfo("Network", "First boot: clearing previously saved Wi-Fi credentials");
        wifiManager_.resetSettings();
        if (preferences.putBool("wifi_init", true) != sizeof(bool)) {
            logError("Network", "Could not save Wi-Fi initialization marker");
            wifiSetupStatus_ = WifiSetupStatus::ERROR;
            preferences.end();
            return;
        }
    }
    preferences.end();

    // WiFi.SSID() only reports the AP we are currently associated with, so it is
    // always empty at boot. Ask WiFiManager for the SSID stored in NVS instead.
    if (!wifiManager_.getWiFiIsSaved()) {
        startConfigPortal(PortalReason::FIRST_SETUP);
    } else {
        wifiSetupStatus_ = WifiSetupStatus::CONNECTING;
        connectStartedAtMs_ = millis();
        WiFi.begin();
        snprintf(pendingSsid_, sizeof(pendingSsid_), "%s", wifiManager_.getWiFiSSID(true).c_str());
        Serial.printf("[%10lu ms] [INFO ] [Network  ] Connecting to saved Wi-Fi \"%s\"\n",
                      static_cast<unsigned long>(millis()), pendingSsid_);
    }
    logInfo("Network", "Network manager initialized");
}
void NetworkManager::setCommandCallback(MqttMessageCallback callback) {
    userCommandCallback = callback;
}
void NetworkManager::callbackThunk(char* topic, byte* payload, unsigned int length) {
    if (userCommandCallback) userCommandCallback(topic, payload, length);
}
bool NetworkManager::wifiConnected() const { return WiFi.status() == WL_CONNECTED; }
bool NetworkManager::mqttConnected() { return mqttClient_.connected(); }
String NetworkManager::wifiSsid() const { return WiFi.SSID(); }

uint8_t NetworkManager::portalClientCount() const {
    return configPortalStarted_ ? WiFi.softAPgetStationNum() : 0;
}

// Runs inside wifiManager_.process() right before it blocks to join the new network.
void NetworkManager::onPortalSave() {
    snprintf(pendingSsid_, sizeof(pendingSsid_), "%s",
             wifiManager_.server ? wifiManager_.server->arg("s").c_str() : "");
    wifiSetupStatus_ = WifiSetupStatus::VERIFYING;
    connectStartedAtMs_ = millis();
    Serial.printf("[%10lu ms] [INFO ] [Network  ] Trying Wi-Fi \"%s\" from setup portal\n",
                  static_cast<unsigned long>(millis()), pendingSsid_);
    if (statusListener_) statusListener_();
}

void NetworkManager::startConfigPortal(PortalReason reason) {
    if (wifiManager_.getConfigPortalActive()) {
        wifiManager_.stopConfigPortal();
    }
    portalReason_ = reason;
    configPortalStarted_ = true;
    wifiSetupStatus_ = WifiSetupStatus::PORTAL;
    connectStartedAtMs_ = 0;
    logInfo("Network", "Wi-Fi setup portal started; connect to its displayed AP and open 192.168.4.1");
    wifiManager_.startConfigPortal(wifiPortalSsid_);
}

void NetworkManager::resetWifiCredentials() {
    logWarn("Network", "Clearing saved Wi-Fi credentials and reopening setup portal");
    mqttClient_.disconnect();
    wifiManager_.resetSettings();
    configPortalStarted_ = false;
    WiFi.mode(WIFI_STA);
    startConfigPortal(PortalReason::USER_RESET);
}

void NetworkManager::maintainResetButton(uint32_t now) {
    const bool pressed = digitalRead(WIFI_RESET_BUTTON_PIN) == LOW;
    if (!pressed) {
        resetButtonDown_ = false;
        resetButtonHandled_ = false;
        resetButtonPressedAtMs_ = 0;
        return;
    }
    if (!resetButtonDown_) {
        resetButtonDown_ = true;
        resetButtonHandled_ = false;
        resetButtonPressedAtMs_ = now;
    }
    if (!resetButtonHandled_ && now - resetButtonPressedAtMs_ >= WIFI_RESET_HOLD_MS) {
        resetButtonHandled_ = true;
        resetWifiCredentials();
    }
}

void NetworkManager::maintain() {
    uint32_t now = millis();
    maintainResetButton(now);
    if (configPortalStarted_) {
        if (wifiManager_.process()) {
            logInfo("Network", "Wi-Fi credentials saved from setup portal");
        } else if (wifiSetupStatus_ == WifiSetupStatus::VERIFYING && !wifiConnected()) {
            // WiFiManager keeps the non-blocking portal open after a failed attempt.
            logWarn("Network", "Could not join the network entered in the setup portal");
            wifiSetupStatus_ = WifiSetupStatus::PORTAL;
            portalReason_ = PortalReason::NEW_WIFI_FAILED;
            connectStartedAtMs_ = 0;
        }
    }

    if (wifiConnected()) {
        // On a successful portal save, process() already shuts the portal down and
        // frees its web server; calling stopConfigPortal() again would dereference
        // that freed server and crash. Only stop it if it is genuinely still open
        // (e.g. the saved network came back while the portal was showing).
        if (configPortalStarted_ && wifiManager_.getConfigPortalActive() &&
            !wifiManager_.stopConfigPortal()) {
            logWarn("Network", "Wi-Fi connected, but the setup access point did not shut down cleanly");
        }
        wifiSetupStatus_ = WifiSetupStatus::CONNECTED;
        portalReason_ = PortalReason::NONE;
        configPortalStarted_ = false;
        connectStartedAtMs_ = 0;
    } else if (!configPortalStarted_ && wifiSetupStatus_ != WifiSetupStatus::ERROR) {
        if (connectStartedAtMs_ == 0) {
            connectStartedAtMs_ = now;
            wifiSetupStatus_ = WifiSetupStatus::CONNECTING;
        }
        if (now - lastWifiAttemptMs_ >= WIFI_RETRY_INTERVAL_MS) {
            lastWifiAttemptMs_ = now;
            logInfo("Network", "Retrying saved Wi-Fi connection");
            WiFi.reconnect();
        }
        if (now - connectStartedAtMs_ >= WIFI_CONNECT_TIMEOUT_MS) {
            startConfigPortal(PortalReason::SAVED_UNREACHABLE);
        }
    }

    if (wifiConnected() && !mqttClient_.connected() &&
        now - lastMqttAttemptMs_ >= MQTT_RETRY_INTERVAL_MS) {
        lastMqttAttemptMs_ = now;
        logInfo("Network", "Connecting to MQTT broker...");
        bool ok;
        if (MQTT_USERNAME[0] != '\0') {
            ok = mqttClient_.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD,
                                     MQTT_STATUS_TOPIC, 1, true, "offline");
        } else {
            ok = mqttClient_.connect(MQTT_CLIENT_ID, MQTT_STATUS_TOPIC, 1, true, "offline");
        }
        if (ok) {
            logInfo("Network", "MQTT connected");
            mqttClient_.publish(MQTT_STATUS_TOPIC, "online", true);
            subscribeCommandTopic();
        } else {
            Serial.printf("[%10lu ms] [WARN ] [Network  ] MQTT connect failed, state=%d\n",
                          static_cast<unsigned long>(millis()), mqttClient_.state());
        }
    }
    if (mqttClient_.connected()) mqttClient_.loop();
}
void NetworkManager::subscribeCommandTopic() {
    if (mqttClient_.subscribe(MQTT_COMMAND_TOPIC, 1)) {
        logInfo("Network", "Subscribed to command topic (QoS 1)");
    } else {
        logError("Network", "MQTT subscribe failed");
    }
}
bool NetworkManager::publish(const char* topic, const char* payload) {
    if (!mqttClient_.connected()) return false;
    return mqttClient_.publish(topic, payload);
}
