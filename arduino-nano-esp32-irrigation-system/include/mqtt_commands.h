#pragma once
#include <Arduino.h>
void handleMqttCommand(char* topic, byte* payload, unsigned int length);
