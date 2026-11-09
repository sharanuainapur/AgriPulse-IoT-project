#include "network_manager.h"
#include "config.h"
#include "logger.h"

static MqttMessageCallback userCommandCallback = nullptr;

void NetworkManager::begin() {
    WiFi.mode(WIFI_STA);
    // Modem sleep: the radio dozes between AP beacons while staying associated,
    // so MQTT commands still arrive with only beacon-interval latency.
    WiFi.setSleep(WIFI_PS_MIN_MODEM);
    mqttClient_.setServer(MQTT_HOST, MQTT_PORT);
    mqttClient_.setBufferSize(MQTT_BUFFER_SIZE);
    mqttClient_.setKeepAlive(30);
    mqttClient_.setSocketTimeout(5);
    mqttClient_.setCallback(NetworkManager::callbackThunk);
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
void NetworkManager::maintain() {
    uint32_t now = millis();
    if (!wifiConnected() && now - lastWifiAttemptMs_ >= WIFI_RETRY_INTERVAL_MS) {
        lastWifiAttemptMs_ = now;
        logInfo("Network", "Connecting to Wi-Fi...");
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
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
