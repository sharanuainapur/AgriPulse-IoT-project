#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiManager.h>
#include "app_state.h"
using MqttMessageCallback = void (*)(char*, byte*, unsigned int);
// Called from inside WiFiManager while the network task is blocked joining a
// new network, so other tasks (the display) can still see the new status.
using NetworkStatusListener = void (*)();

class NetworkManager {
public:
    void begin();
    void maintain();
    bool wifiConnected() const;
    bool mqttConnected();
    int mqttState() { return mqttClient_.state(); } // PubSubClient MQTT_* state code
    bool publish(const char* topic, const char* payload);
    void subscribeCommandTopic();
    void setCommandCallback(MqttMessageCallback callback);
    void setStatusListener(NetworkStatusListener listener) { statusListener_ = listener; }
    WifiSetupStatus wifiSetupStatus() const { return wifiSetupStatus_; }
    String wifiSsid() const;
    const char* wifiPortalSsid() const { return wifiPortalSsid_; }
    const char* pendingSsid() const { return pendingSsid_; }
    PortalReason portalReason() const { return portalReason_; }
    uint32_t connectStartedAtMs() const { return connectStartedAtMs_; }
    uint8_t portalClientCount() const;
private:
    void startConfigPortal(PortalReason reason);
    void onPortalSave();
    void resetWifiCredentials();
    void maintainResetButton(uint32_t now);

    WiFiClient wifiClient_;
    PubSubClient mqttClient_{wifiClient_};
    WiFiManager wifiManager_;
    uint32_t lastWifiAttemptMs_ = 0;
    uint32_t lastMqttAttemptMs_ = 0;
    uint32_t connectStartedAtMs_ = 0;
    uint32_t resetButtonPressedAtMs_ = 0;
    bool configPortalStarted_ = false;
    bool resetButtonDown_ = false;
    bool resetButtonHandled_ = false;
    WifiSetupStatus wifiSetupStatus_ = WifiSetupStatus::STARTING;
    char wifiPortalSsid_[33] = {};
    char pendingSsid_[33] = {};
    PortalReason portalReason_ = PortalReason::NONE;
    NetworkStatusListener statusListener_ = nullptr;
    static void callbackThunk(char* topic, byte* payload, unsigned int length);
};
