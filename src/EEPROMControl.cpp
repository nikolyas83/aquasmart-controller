/*
  EEPROMControl.cpp
  Оновлено: Додано збереження налаштувань MQTT
*/

#include "EEPROMControl.h"
#include <Preferences.h>

static Preferences prefs;

void initEEPROM() {
  prefs.begin("aquasmart", false);
  if (!prefs.isKey("http_user")) prefs.putString("http_user", "admin");
  if (!prefs.isKey("http_pass")) prefs.putString("http_pass", "admin123");
  if (!prefs.isKey("mqtt_server")) prefs.putString("mqtt_server", "broker.mqtt.com");
  if (!prefs.isKey("mqtt_port")) prefs.putInt("mqtt_port", 1883);
  if (!prefs.isKey("mqtt_user")) prefs.putString("mqtt_user", "aquasmart");
  if (!prefs.isKey("mqtt_pass")) prefs.putString("mqtt_pass", "password123");
}

void saveToEEPROM() {
  // Додаткові налаштування можна зберігати тут
}

void loadFromEEPROM() {
  // Завантаження налаштувань MQTT у MQTTControl
}