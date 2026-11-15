#include "MQTTControl.h"
#include <PubSubClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "ScheduleControl.h"
#include "TemperatureControl.h"
#include "RTC.h"
#include "LightControl.h"
#include "PresetControl.h"

WiFiClient espClient;
PubSubClient client(espClient);

static String mqttServer = "";
static int mqttPort = 1883;
static String mqttUser = "";
static String mqttPass = "";
static bool mqttEnabled = false;

static unsigned long lastReconnectAttempt = 0;
static unsigned long lastStatusPublish = 0;
const unsigned long MQTT_RECONNECT_INTERVAL = 15000;
const unsigned long MQTT_STATUS_INTERVAL = 30000;

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  JsonDocument doc;
  // Парсим сырой буфер напрямую без посимвольного создания String
  DeserializationError error = deserializeJson(doc, payload, length);
  if (error) {
    Serial.println("Failed to parse MQTT message: " + String(error.c_str()));
    return;
  }

  if (!doc["command"].is<String>()) return;
  String command = doc["command"].as<String>();

  if (command == "set_brightness" && doc["channel"].is<int>() && doc["value"].is<int>()) {
    int channel = doc["channel"];
    int value = doc["value"];
    setChannelBrightness(channel, value);
    publishStatus();
  } 
  else if (command == "load_preset" && doc["name"].is<String>()) {
    String presetName = doc["name"].as<String>();
    if (loadLightingPreset(presetName)) {
      publishStatus();
    }
  }
  else if (command == "effect" && doc["type"].is<String>()) {
    String type = doc["type"].as<String>();
    int duration = doc["duration"] | 15;
    if (type == "sunrise") startSunrise(duration);
    else if (type == "sunset") startSunset(duration);
    else if (type == "night") setNightMode();
    publishStatus();
  }
  else if (command == "set_time" && doc["year"].is<int>() && doc["month"].is<int>() &&
           doc["day"].is<int>() && doc["hour"].is<int>() && doc["minute"].is<int>() &&
           doc["second"].is<int>()) {
    setManualTime(doc["year"], doc["month"], doc["day"], doc["hour"], doc["minute"], doc["second"]);
  } 
  else if (command == "set_day_schedule" && doc["date"].is<String>() && doc["schedule"].is<JsonArray>()) {
    String date = doc["date"].as<String>();
    String schedJson;
    serializeJson(doc["schedule"], schedJson);
    setDaySchedule(date, schedJson);
  }
}

void setMQTTConfig(const String& server, int port, const String& user, const String& pass) {
  mqttServer = server;
  mqttPort = (port > 0) ? port : 1883;
  mqttUser = user;
  mqttPass = pass;

  if (mqttServer.isEmpty()) {
    mqttEnabled = false;
    if (client.connected()) client.disconnect();
    Serial.println("MQTT disabled");
    return;
  }

  // Сразу применяем новые настройки без перезагрузки платы
  if (client.connected()) {
    client.disconnect();
  }
  client.setServer(mqttServer.c_str(), mqttPort);
  client.setCallback(mqttCallback);
  mqttEnabled = true;
  lastReconnectAttempt = 0;
  Serial.println("MQTT config applied: " + mqttServer + ":" + String(mqttPort));
}

void publishStatus() {
  if (!client.connected() || !mqttEnabled) return;

  JsonDocument doc;
  doc["temp"] = getTemperature();
  doc["ssid"] = WiFi.SSID();
  doc["rssi"] = WiFi.RSSI();
  doc["datetime"] = getCurrentDateTime();

  JsonArray ch = doc["channels"].to<JsonArray>();
  for (int i = 0; i < 5; i++) {
    ch.add(getChannelBrightness(i));
  }

  String json;
  serializeJson(doc, json);
  client.publish("aquasmart/status", json.c_str(), true); // retained = true
}

void reconnectMQTT() {
  if (!mqttEnabled || mqttServer.isEmpty()) return;
  if (WiFi.status() != WL_CONNECTED) return;

  unsigned long now = millis();
  if (lastReconnectAttempt != 0 && (now - lastReconnectAttempt < MQTT_RECONNECT_INTERVAL)) {
    return;
  }
  lastReconnectAttempt = now;

  String clientId = "AquaSmart-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  Serial.println("Connecting to MQTT broker " + mqttServer + "...");

  bool connected = false;
  if (mqttUser.isEmpty()) {
    connected = client.connect(clientId.c_str());
  } else {
    connected = client.connect(clientId.c_str(), mqttUser.c_str(), mqttPass.c_str());
  }

  if (connected) {
    Serial.println("Connected to MQTT");
    client.subscribe("aquasmart/control");
    publishStatus();
    lastStatusPublish = now;
  } else {
    Serial.println("MQTT connection failed, rc=" + String(client.state()));
  }
}

void initMQTT() {
  espClient.setTimeout(2); // Короткий таймаут сокета (2 сек вместо 15), чтобы не вешать задачу
  client.setBufferSize(1024); // Увеличиваем буфер для приёма расписаний и отправки статуса

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
    return;
  }

  client.loop();

  // Периодическая отправка телеметрии раз в 30 секунд
  unsigned long now = millis();
  if (now - lastStatusPublish >= MQTT_STATUS_INTERVAL) {
    lastStatusPublish = now;
    publishStatus();
  }
}
