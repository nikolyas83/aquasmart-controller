#ifndef AQUA_WIFI_CONTROL_H
#define AQUA_WIFI_CONTROL_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

void initWiFi();
void handleWiFi();
void requestWiFiConnect(const String& ssid, const String& password);
String getAvailableNetworks();
void forceScanNetworks(AsyncWebServerRequest *request);

#endif