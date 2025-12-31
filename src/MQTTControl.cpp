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

void setMQTTConfig(const String& server, int port, const String& user, const String& pass) {
  mqttServer = server;
  mqttPort = port;
  mqttUser = user;
  mqttPass = pass;
  mqttEnabled = true;
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
  String clientId = "AquaSmart-" + String(random(0xffff), HEX);
  Serial.println("Connecting to MQTT...");
  if (client.connect(clientId.c_str(), mqttUser.c_str(), mqttPass.c_str())) {
    Serial.println("Connected to MQTT");
    client.subscribe("aquasmart/control");
    publishStatus();
  } else {
    Serial.println("MQTT connection failed, rc=" + String(client.state()));
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