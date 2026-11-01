#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
using MqttMessageCallback = void (*)(char*, byte*, unsigned int);

class NetworkManager {
public:
    void begin();
    void maintain();
    bool wifiConnected() const;
    bool mqttConnected();
    bool publish(const char* topic, const char* payload);
    void subscribeCommandTopic();
    void setCommandCallback(MqttMessageCallback callback);
private:
    WiFiClient wifiClient_;
    PubSubClient mqttClient_{wifiClient_};
    uint32_t lastWifiAttemptMs_ = 0;
    uint32_t lastMqttAttemptMs_ = 0;
    static void callbackThunk(char* topic, byte* payload, unsigned int length);
};
