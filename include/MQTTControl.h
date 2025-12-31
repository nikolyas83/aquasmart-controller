#ifndef MQTT_CONTROL_H
#define MQTT_CONTROL_H

#include <Arduino.h>

void initMQTT();
void handleMQTT();
void publishStatus();
void setMQTTConfig(const String& server, int port, const String& user, const String& pass);

#endif
