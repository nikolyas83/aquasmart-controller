#include "MQTTControl.h"
#include <PubSubClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "ScheduleControl.h"
#include "TemperatureControl.h"
#include "RTC.h"
#include "LightControl.h"

WiFiClient espClient;
PubSubClient client(espClient);
static String mqttServer = "";
static int mqttPort = 1883;
static String mqttUser = "";
static String mqttPass = "";
static bool mqttEnabled = false;
static unsigned long lastReconnectAttempt = 0;
static unsigned long reconnectDelay = 5000;  // Почнемо з 5 сек
const unsigned long MAX_RECONNECT_DELAY = 60000;  // Макс 60 сек

void setMQTTConfig(const String& server, int port, const String& user, const String& pass) {
  if (server.length() > 256) {
    Serial.println("MQTT server address too long");
    return;
  }
  if (user.length() > 100 || pass.length() > 100) {
    Serial.println("MQTT credentials too long");
    return;
  }
  
  mqttServer = server;
  mqttPort = port;
  mqttUser = user;
  mqttPass = pass;
  mqttEnabled = true;
  reconnectDelay = 5000;  // Скидаємо затримку
  Serial.println("MQTT config set: " + server + ":" + String(port));
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println("MQTT message on " + String(topic) + ": " + message);

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, message);
  if (error) {
    Serial.println("Failed to parse MQTT message: " + String(error.c_str()));
    return;
  }

  if (doc["command"].is<String>()) {
    String command = doc["command"];
    if (command == "set_brightness" && doc["channel"].is<int>() && doc["value"].is<int>()) {
      int channel = doc["channel"];
      int value = doc["value"];
      setChannelBrightness(channel, value);
      Serial.println("Set brightness from MQTT: channel=" + String(channel) + ", value=" + String(value));
    } else if (command == "set_time" && doc["year"].is<int>() && doc["month"].is<int>() &&
               doc["day"].is<int>() && doc["hour"].is<int>() && doc["minute"].is<int>() &&
               doc["second"].is<int>()) {
      setManualTime(doc["year"], doc["month"], doc["day"], doc["hour"], doc["minute"], doc["second"]);
      Serial.println("Set time from MQTT");
    } else if (command == "add_schedule") {
      String event = doc["event"];
      addScheduleEvent(event);
      Serial.println("Added schedule event from MQTT");
    } else if (command == "remove_schedule" && doc["id"].is<int>()) {
      int id = doc["id"];
      removeScheduleEvent(id);
      Serial.println("Removed schedule event from MQTT: id=" + String(id));
    }
  }
}

void publishStatus() {
  if (!client.connected() || !mqttEnabled) return;
  JsonDocument doc;
  doc["temp"] = getTemperature();
  doc["ssid"] = WiFi.SSID();
  doc["rssi"] = WiFi.RSSI();
  doc["datetime"] = getCurrentDateTime();
  String json;
  serializeJson(doc, json);
  client.publish("aquasmart/status", json.c_str());
  Serial.println("Published status: " + json);
}

void reconnectMQTT() {
  if (!mqttEnabled) return;
  if (client.connected()) return;
  
  // Перевіряємо Wi-Fi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, skipping MQTT reconnect");
    return;
  }
  
  // Перевіряємо затримку між спробами
  if (millis() - lastReconnectAttempt < reconnectDelay) {
    return;
  }
  
  lastReconnectAttempt = millis();
  String clientId = "AquaSmart-" + String(random(0xffff), HEX);
  
  Serial.println("Connecting to MQTT... (attempt, delay=" + String(reconnectDelay) + "ms)");
  
  if (client.connect(clientId.c_str(), mqttUser.c_str(), mqttPass.c_str())) {
    Serial.println("Connected to MQTT");
    client.subscribe("aquasmart/control");
    reconnectDelay = 5000;  // Скидаємо затримку при успішному підключенні
    publishStatus();
  } else {
    int rc = client.state();
    Serial.println("MQTT connection failed, rc=" + String(rc));
    
    // Експоненціальна затримка (backoff)
    reconnectDelay = min(reconnectDelay * 2, MAX_RECONNECT_DELAY);
    
    // Розбір кодів помилок
    switch (rc) {
      case -1: Serial.println("  -> Connection timeout"); break;
      case -2: Serial.println("  -> Connection lost"); break;
      case -3: Serial.println("  -> Connection refused"); break;
      case 1: Serial.println("  -> Bad protocol"); break;
      case 2: Serial.println("  -> Bad client ID"); break;
      case 3: Serial.println("  -> Server unavailable"); break;
      case 4: Serial.println("  -> Bad username/password"); break;
      case 5: Serial.println("  -> Not authorized"); break;
      default: Serial.println("  -> Unknown error"); break;
    }
  }
}

void initMQTT() {
  if (mqttServer.isEmpty()) {
    mqttEnabled = false;
    Serial.println("MQTT disabled: no server configured");
    return;
  }
  client.setServer(mqttServer.c_str(), mqttPort);
  client.setCallback(mqttCallback);
  mqttEnabled = true;
  Serial.println("MQTT initialized");
}

void handleMQTT() {
  if (!mqttEnabled) return;
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();
}