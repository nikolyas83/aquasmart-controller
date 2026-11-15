#include "EEPROMControl.h"
#include <Preferences.h>
#include "MQTTControl.h"

// Подключаем глобальные переменные авторизации из WebServerControl.cpp
extern String http_username;
extern String http_password;

static Preferences prefs;

void initEEPROM() {
  if (!prefs.begin("aquasmart", false)) {
    Serial.println("Error: Failed to open NVS preferences");
    return;
  }

  if (!prefs.isKey("http_user"))   prefs.putString("http_user", "admin");
  if (!prefs.isKey("http_pass"))   prefs.putString("http_pass", "admin123");
  if (!prefs.isKey("mqtt_server")) prefs.putString("mqtt_server", ""); // По умолчанию выключен
  if (!prefs.isKey("mqtt_port"))   prefs.putInt("mqtt_port", 1883);
  if (!prefs.isKey("mqtt_user"))   prefs.putString("mqtt_user", "");
  if (!prefs.isKey("mqtt_pass"))   prefs.putString("mqtt_pass", "");

  prefs.end();
  Serial.println("NVS Preferences initialized");

  // Сразу применяем сохранённые настройки в модули
  loadFromEEPROM();
}

void loadFromEEPROM() {
  if (!prefs.begin("aquasmart", true)) { // true = режим только для чтения
    return;
  }

  http_username = prefs.getString("http_user", "admin");
  http_password = prefs.getString("http_pass", "admin123");

  String server = prefs.getString("mqtt_server", "");
  int port      = prefs.getInt("mqtt_port", 1883);
  String user   = prefs.getString("mqtt_user", "");
  String pass   = prefs.getString("mqtt_pass", "");

  prefs.end();

  if (!server.isEmpty()) {
    setMQTTConfig(server, port, user, pass);
  }
  Serial.println("Settings loaded from NVS");
}

void saveToEEPROM() {
  // Сохраняем текущие настройки авторизации (можно расширить сохранением MQTT)
  if (!prefs.begin("aquasmart", false)) {
    return;
  }

  prefs.putString("http_user", http_username);
  prefs.putString("http_pass", http_password);

  prefs.end();
  Serial.println("Settings saved to NVS");
}

// Удобная функция для вызова из WebServerControl при сохранении формы MQTT
void saveMQTTSettings(const String& server, int port, const String& user, const String& pass) {
  if (!prefs.begin("aquasmart", false)) {
    return;
  }

  prefs.putString("mqtt_server", server);
  prefs.putInt("mqtt_port", port);
  prefs.putString("mqtt_user", user);
  prefs.putString("mqtt_pass", pass);

  prefs.end();
  setMQTTConfig(server, port, user, pass);
  Serial.println("MQTT settings saved to NVS");
}
